#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "LauncherState.h"
#include "LauncherView.h"
#include "UpdateTypes.h"

class LauncherPresenter
{
public:
    LauncherPresenter();
    ~LauncherPresenter();

    LauncherPresenter(const LauncherPresenter&) = delete;
    LauncherPresenter& operator=(const LauncherPresenter&) = delete;

    void Initialize() noexcept;
    void Update() noexcept;
    void Shutdown() noexcept;

    LauncherViewEvents CreateViewEvents() noexcept;

    bool ShouldClose() const noexcept { return shouldClose_; }
    void RequestClose() noexcept { shouldClose_ = true; }

    void OnPlay() noexcept;
    void OnCheckFiles() noexcept;
    void OnOption() noexcept;
    void OnExit() noexcept;
    void OnOpenLink(const std::string& url) noexcept;

private:
    std::vector<std::string> GetEndpoints() const;
    std::string FetchFromCDN(const std::string& path) const;
    void CheckUpdatesAsync(bool isFullCheck);
    bool RunInstaller(const std::vector<Arquivo>& files, int pendingUpdateCount);
    void LaunchGame();
    void CheckSelfUpdate();
    std::filesystem::path GetGamePath() const;

    std::thread workerThread_;
    std::atomic<bool> isWorkerDone_{ false };
    std::atomic<bool> isRunning_{ false };
    bool isMaintenance_ = false;
    bool isVerifying_ = false;
    bool shouldClose_ = false;

    int localVersion_ = 0;
    int currentVersion_ = 0;
    int updateCount_ = 0;
    std::vector<Arquivo> fileList_;
};
