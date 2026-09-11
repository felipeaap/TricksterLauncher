#include "Logger.h"

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace Logger
{
namespace
{
    std::mutex   s_mutex;
    std::ofstream s_logFile;

    std::string Timestamp() noexcept
    {
        try
        {
            const auto now = std::chrono::system_clock::now();
            const auto t   = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_s(&tm, &t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }
        catch (...) { return "??"; }
    }

    void WriteLine(const char* level, const std::string& msg) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> lk(s_mutex);
            const std::string line = "[" + Timestamp() + "] [" + level + "] " + msg + "\n";
            if (s_logFile.is_open())
            {
                s_logFile << line;
                s_logFile.flush();
            }
            OutputDebugStringA(line.c_str());
        }
        catch (...) {}
    }
} // namespace

void Init(const std::string& path) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        s_logFile.open(path, std::ios::app);
        if (s_logFile.is_open())
            s_logFile << "\n--- Launcher started ---\n";
    }
    catch (...) {}
}

void Shutdown() noexcept
{
    try
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        if (s_logFile.is_open())
        {
            s_logFile << "--- Launcher stopped ---\n";
            s_logFile.close();
        }
    }
    catch (...) {}
}

void Log(const std::string& msg) noexcept
{
#if defined(_DEBUG) || defined(LAUNCHER_DEBUG_LOG)
    WriteLine("INFO", msg);
#else
    (void)msg;
#endif
}

void LogError(const std::string& msg) noexcept
{
    WriteLine("ERROR", msg);
}

} // namespace Logger
