#pragma once
#include <string>

/// Lightweight application logger.
///
/// - Logger::Log()      -> diagnostic message; compiled out in Release unless LAUNCHER_DEBUG_LOG is defined.
/// - Logger::LogError() -> always written to the error log file and OutputDebugStringA.
///
/// Call Init() once at startup and Shutdown() before exit.
namespace Logger
{
    /// Open the log file and begin logging.
    void Init(const std::string& path) noexcept;

    /// Flush and close the log file.
    void Shutdown() noexcept;

    /// Diagnostic trace — compiled out in Release builds (no-op).
    void Log(const std::string& msg) noexcept;

    /// Error log — always written regardless of build configuration.
    void LogError(const std::string& msg) noexcept;
}
