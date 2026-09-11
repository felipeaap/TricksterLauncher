#pragma once

#include <atomic>
#include <mutex>
#include <string>

/// Snapshot of the state passed directly to the view for rendering.
struct LauncherViewState
{
    float fileProgress = 0.0f;
    float totalProgress = 0.0f;
    std::string fileString;
    std::string speedString;
    bool isGameEnabled = false;
    bool isCheckEnabled = false;
    bool isOptionEnabled = false;
    bool isMaintenance = false;
};

/// Shared launcher state written by worker/presenter threads and read by the render thread.
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

    /// Button enabled states
    extern std::atomic<bool> isGameEnabled;
    extern std::atomic<bool> isCheckEnabled;
    extern std::atomic<bool> isOptionEnabled;
    extern std::atomic<bool> isMaintenance;

    /// Obtains a consistent snapshot for the view layer.
    LauncherViewState GetSnapshot();

    /// Convenience helpers
    void SetProgress(float fileProg, float totalProg);
    void SetStatus(const std::string& status);
    void SetSpeed(const std::string& speed);
    void SetButtons(bool game, bool check, bool option);
    void SetMaintenance(bool maintenance);
}
