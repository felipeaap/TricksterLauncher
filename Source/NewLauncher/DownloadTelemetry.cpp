#include "DownloadTelemetry.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>

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
}

namespace download_telemetry
{
void Record(const DownloadTelemetryRecord& record) noexcept
{
    try
    {
        const auto path = TelemetryPath();
        if (path.empty())
            return;

        const std::lock_guard lock(g_mutex);
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
}
