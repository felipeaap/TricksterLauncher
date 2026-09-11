#include "LauncherState.h"

namespace LauncherState
{
    std::atomic<float> fileProgress{ 0.0f };
    std::atomic<float> totalProgress{ 0.0f };
    std::mutex fileStringMutex;
    std::string fileString;
    std::mutex speedStringMutex;
    std::string speedString;

    std::atomic<bool> isGameEnabled{ false };
    std::atomic<bool> isCheckEnabled{ false };
    std::atomic<bool> isOptionEnabled{ false };
    std::atomic<bool> isMaintenance{ false };

    LauncherViewState GetSnapshot()
    {
        LauncherViewState state;
        state.fileProgress = fileProgress.load(std::memory_order_relaxed);
        state.totalProgress = totalProgress.load(std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(fileStringMutex);
            state.fileString = fileString;
        }

        {
            std::lock_guard<std::mutex> lock(speedStringMutex);
            state.speedString = speedString;
        }

        state.isGameEnabled = isGameEnabled.load(std::memory_order_relaxed);
        state.isCheckEnabled = isCheckEnabled.load(std::memory_order_relaxed);
        state.isOptionEnabled = isOptionEnabled.load(std::memory_order_relaxed);
        state.isMaintenance = isMaintenance.load(std::memory_order_relaxed);

        return state;
    }

    void SetProgress(float fileProg, float totalProg)
    {
        fileProgress.store(fileProg, std::memory_order_relaxed);
        totalProgress.store(totalProg, std::memory_order_relaxed);
    }

    void SetStatus(const std::string& status)
    {
        std::lock_guard<std::mutex> lock(fileStringMutex);
        fileString = status;
    }

    void SetSpeed(const std::string& speed)
    {
        std::lock_guard<std::mutex> lock(speedStringMutex);
        speedString = speed;
    }

    void SetButtons(bool game, bool check, bool option)
    {
        isGameEnabled.store(game, std::memory_order_relaxed);
        isCheckEnabled.store(check, std::memory_order_relaxed);
        isOptionEnabled.store(option, std::memory_order_relaxed);
    }

    void SetMaintenance(bool maintenance)
    {
        isMaintenance.store(maintenance, std::memory_order_relaxed);
    }
}
