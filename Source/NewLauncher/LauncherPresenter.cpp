#include "LauncherPresenter.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "Config.h"
#include "EndpointManager.h"
#include "GameLauncher.h"
#include "Language.h"
#include "LauncherState.h"
#include "LauncherUpdater.h"
#include "Logger.h"
#include "UpdateCoordinator.h"
#include "UpdateInstaller.h"
#include "VersionManager.h"

LauncherPresenter::LauncherPresenter() = default;

LauncherPresenter::~LauncherPresenter()
{
    Shutdown();
}

std::vector<std::string> LauncherPresenter::GetEndpoints() const
{
    std::vector<std::string> endpoints;
    if (!config::LauncherCDN.empty())
        endpoints.push_back(config::LauncherCDN);

    for (const auto& backup : config::LauncherCDNBackups)
    {
        if (!backup.empty() && std::find(endpoints.begin(), endpoints.end(), backup) == endpoints.end())
            endpoints.push_back(backup);
    }
    return endpoints;
}

std::string LauncherPresenter::FetchFromCDN(const std::string& path) const
{
    EndpointManager endpoints(GetEndpoints(), config::IsCDNUsingSSL);
    return endpoints.Get(path);
}

std::filesystem::path LauncherPresenter::GetGamePath() const
{
    char buffer[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

void LauncherPresenter::Initialize() noexcept
{
    LauncherState::SetStatus(lang::GetString("launcher_checking"));
    LauncherState::SetSpeed("");
    LauncherState::SetProgress(0.0f, 0.0f);
    LauncherState::SetButtons(false, false, false);

    // Check maintenance status
    const std::string maintenanceResponse = FetchFromCDN("/maintenance.txt");
    isMaintenance_ = (maintenanceResponse == "true");
    LauncherState::SetMaintenance(isMaintenance_);

    if (!isVerifying_)
    {
        CheckSelfUpdate();
        CheckUpdatesAsync(false);
        isVerifying_ = true;
    }
}

void LauncherPresenter::Update() noexcept
{
    if (isWorkerDone_.load(std::memory_order_acquire))
    {
        const bool canPlay = !isMaintenance_;
        LauncherState::SetButtons(canPlay, true, true);
    }
    else
    {
        LauncherState::SetButtons(false, false, false);
    }
}

void LauncherPresenter::Shutdown() noexcept
{
    isRunning_.store(false, std::memory_order_release);
    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}

LauncherViewEvents LauncherPresenter::CreateViewEvents() noexcept
{
    LauncherViewEvents events;
    events.onPlayClicked = [this]() { OnPlay(); };
    events.onConnectClicked = [this](const std::string& account, const std::string& password, bool saveAccount)
    {
        OnConnect(account, password, saveAccount);
    };
    events.onCheckClicked = [this]() { OnCheckFiles(); };
    events.onOptionClicked = [this]() { OnOption(); };
    events.onExitClicked = [this]() { OnExit(); };
    events.onLinkClicked = [this](const std::string& url) { OnOpenLink(url); };
    return events;
}

void LauncherPresenter::OnPlay() noexcept
{
    // Game start clicked - View will transition to login form
}

void LauncherPresenter::OnConnect(
    const std::string& account,
    const std::string& password,
    bool saveAccount) noexcept
{
    (void)saveAccount;
    if (isWorkerDone_.load(std::memory_order_acquire) && !isMaintenance_)
    {
        LaunchGame(account, password);
    }
}

void LauncherPresenter::OnCheckFiles() noexcept
{
    LauncherState::SetButtons(false, false, false);
    CheckUpdatesAsync(true);
}

void LauncherPresenter::OnOption() noexcept
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi{};
    const std::filesystem::path gameExe = GetGamePath() / config::OptionExecName.c_str();
    const std::string exePath = gameExe.string();
    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        MessageBoxA(nullptr, lang::GetString("launcher_setup_fail").c_str(), "Error!", MB_OK);
    }
}

void LauncherPresenter::OnExit() noexcept
{
    shouldClose_ = true;
}

void LauncherPresenter::OnOpenLink(const std::string& url) noexcept
{
    if (!url.empty())
    {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void LauncherPresenter::LaunchGame(const std::string& account, const std::string& password)
{
    GameLauncher::Options options;
    options.injectDll = config::IsDllInjectEnable;
    options.dllPath = config::InjectDLLName;
    options.injectionDelayMilliseconds = 2000;
    options.account = account;
    options.password = password;

    GameLauncher launcher(options);
    if (launcher.Launch(GetGamePath()))
        ExitProcess(0);
}

void LauncherPresenter::CheckSelfUpdate()
{
    const std::string launcherName = "Splash.exe";

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    const std::string currentExe = exePath;
    const std::filesystem::path launcherPath(currentExe);

    LauncherUpdater updater(GetEndpoints(), config::IsCDNUsingSSL);
    const std::string remoteLauncherHash = FetchFromCDN("/launcher.txt");
    if (remoteLauncherHash.empty())
    {
        return;
    }

    updater.Update(
        launcherPath,
        remoteLauncherHash,
        launcherName,
        currentExe);
}

void LauncherPresenter::CheckUpdatesAsync(bool isFullCheck)
{
    if (!fileList_.empty() && !isWorkerDone_.load(std::memory_order_acquire))
        return;

    if (workerThread_.joinable())
        workerThread_.join();

    isWorkerDone_.store(false, std::memory_order_release);
    isRunning_.store(true, std::memory_order_release);

    workerThread_ = std::thread([this, isFullCheck]()
    {
        UpdateCoordinator coordinator(
            [this](const std::string& path)
            {
                return FetchFromCDN(path);
            },
            [](const std::string& fileName)
            {
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("splash_check") + fileName;
            },
            [](float fileProgress, float totalProgress)
            {
                LauncherState::fileProgress.store(fileProgress, std::memory_order_relaxed);
                LauncherState::totalProgress.store(totalProgress, std::memory_order_relaxed);
            });

        {
            std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
            LauncherState::fileString = lang::GetString("launcher_filelist_building");
        }

        localVersion_ = VersionManager::Load();

        coordinator.Check(
            fileList_,
            isFullCheck,
            localVersion_,
            currentVersion_,
            updateCount_);

        const bool success = RunInstaller(fileList_, updateCount_);
        isWorkerDone_.store(success, std::memory_order_release);
        isRunning_.store(false, std::memory_order_release);
        if (success)
        {
            VersionManager::Save(currentVersion_);
        }
    });
}

bool LauncherPresenter::RunInstaller(const std::vector<Arquivo>& files, int pendingUpdateCount)
{
    LauncherState::totalProgress.store(0.0f, std::memory_order_relaxed);

    UpdateInstaller installer(GetEndpoints(), config::IsCDNUsingSSL);
    const bool result = installer.Install(
        files,
        pendingUpdateCount,
        [](float fileProgress, float totalProgress)
        {
            LauncherState::fileProgress.store(fileProgress, std::memory_order_relaxed);
            LauncherState::totalProgress.store(totalProgress, std::memory_order_relaxed);
        },
        [](const std::string& fileName)
        {
            std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
            LauncherState::fileString = lang::GetString("launcher_worker_downloading") + ": " + fileName;
        },
        [](double bytesPerSecond)
        {
            double speed = bytesPerSecond;
            const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };
            int unit = 0;
            while (speed >= 1024.0 && unit < 4)
            {
                speed /= 1024.0;
                ++unit;
            }

            char buffer[64]{};
            std::snprintf(buffer, sizeof(buffer), "%.2f %s", speed, units[unit]);
            std::lock_guard<std::mutex> lock(LauncherState::speedStringMutex);
            LauncherState::speedString = buffer;
        });

    {
        std::lock_guard<std::mutex> lock(LauncherState::speedStringMutex);
        LauncherState::speedString.clear();
    }

    if (!result)
        return false;

    LauncherState::fileProgress.store(1.0f, std::memory_order_relaxed);
    LauncherState::totalProgress.store(1.0f, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
        LauncherState::fileString = isMaintenance_
            ? lang::GetString("launcher_worker_maintenance")
            : lang::GetString("launcher_worker_complete");
    }

    return true;
}
