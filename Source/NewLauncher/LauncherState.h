#pragma once

#include <atomic>
#include <mutex>
#include <string>

/// Shared launcher state written by worker threads and read by the render thread.
/// Replaces the global variables previously exposed in the gui:: namespace.
namespace LauncherState
{
    /// Per-file verification/download progress [0.0, 1.0].
    extern std::atomic<float> fileProgress;

    /// Overall update progress [0.0, 1.0].
    extern std::atomic<float> totalProgress;

    /// Protects fileString.
    extern std::mutex fileStringMutex;

    /// Human-readable current file name displayed on the splash screen.
    extern std::string fileString;

    /// Protects speedString.
    extern std::mutex speedStringMutex;

    /// Human-readable download speed string, e.g. "1.2 MB/s".
    extern std::string speedString;
}
