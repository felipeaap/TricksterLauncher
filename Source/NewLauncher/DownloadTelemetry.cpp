#include "DownloadTelemetry.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace
{
std::mutex g_mutex;

std::filesystem::path TelemetryPath()
{
    wchar_t localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};

    const auto directory = std::filesystem::path(localAppData) / L"TricksterLauncher";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return directory / L"download-telemetry.jsonl";
}

std::string EscapeJson(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += character; break;
        }
    }
    return result;
}

struct HostMetrics
{
    long long totalBytes = 0;
    double totalSeconds = 0.0;
    double ewmaSpeed = 0.0; // bytes per second
    int successes = 0;
    int failures = 0;
    int consecutiveFailures = 0;
};

std::unordered_map<std::string, HostMetrics> g_hostMetrics;

void UpdateHostMetricsLocked(const DownloadTelemetryRecord& record)
{
    auto& metrics = g_hostMetrics[record.host];
    if (record.success)
    {
        metrics.successes++;
        metrics.consecutiveFailures = 0;
        metrics.totalBytes += record.bytes;
        metrics.totalSeconds += record.seconds;
        if (record.seconds > 0.0)
        {
            const double currentSpeed = static_cast<double>(record.bytes) / record.seconds;
            if (metrics.ewmaSpeed <= 0.0)
                metrics.ewmaSpeed = currentSpeed;
            else
                metrics.ewmaSpeed = 0.7 * metrics.ewmaSpeed + 0.3 * currentSpeed; // EWMA alpha=0.3
        }
    }
    else
    {
        metrics.failures++;
        metrics.consecutiveFailures++;
    }
}
}

namespace download_telemetry
{
void Record(const DownloadTelemetryRecord& record) noexcept
{
    try
    {
        const auto path = TelemetryPath();
        const std::lock_guard lock(g_mutex);

        UpdateHostMetricsLocked(record);

        if (path.empty())
            return;

        std::ofstream file(path, std::ios::app | std::ios::binary);
        if (!file)
            return;

        const double bitsPerSecond = record.seconds > 0.0
            ? static_cast<double>(record.bytes) / record.seconds
            : 0.0;

        file << "{\"host\":\"" << EscapeJson(record.host)
             << "\",\"path\":\"" << EscapeJson(record.path)
             << "\",\"bytes\":" << record.bytes
             << ",\"seconds\":" << record.seconds
             << ",\"bytes_per_second\":" << bitsPerSecond
             << ",\"connections\":" << record.connections
             << ",\"ranged\":" << (record.ranged ? "true" : "false")
             << ",\"success\":" << (record.success ? "true" : "false")
             << "}\n";
    }
    catch (...)
    {
        // Telemetry is strictly best-effort and must never affect updating.
    }
}

std::vector<std::string> GetRankedHosts(const std::vector<std::string>& hosts) noexcept
{
    std::vector<std::string> sorted = hosts;
    if (sorted.size() <= 1)
        return sorted;

    try
    {
        const std::lock_guard lock(g_mutex);

        auto scoreHost = [](const std::string& host) -> double
        {
            const auto it = g_hostMetrics.find(host);
            if (it == g_hostMetrics.end())
                return 1000.0; // Untested host gets reasonable baseline score

            const auto& m = it->second;
            if (m.consecutiveFailures >= 3)
                return -100.0 * m.consecutiveFailures; // Heavily penalize broken endpoints

            double score = m.ewmaSpeed > 0.0 ? m.ewmaSpeed : 1000.0;
            // Penalize recent failures
            score /= (1.0 + m.consecutiveFailures * 2.0);
            return score;
        };

        std::stable_sort(sorted.begin(), sorted.end(), [&](const std::string& a, const std::string& b)
        {
            return scoreHost(a) > scoreHost(b);
        });
    }
    catch (...)
    {
    }

    return sorted;
}

double GetHostThroughput(const std::string& host) noexcept
{
    try
    {
        const std::lock_guard lock(g_mutex);
        const auto it = g_hostMetrics.find(host);
        if (it != g_hostMetrics.end())
            return it->second.ewmaSpeed;
    }
    catch (...)
    {
    }
    return 0.0;
}

void ResetHostMetrics() noexcept
{
    try
    {
        const std::lock_guard lock(g_mutex);
        g_hostMetrics.clear();
    }
    catch (...)
    {
    }
}
}
