#include "LauncherPresenter.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "AuthClient.h"
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

bool LauncherPresenter::FetchFromCDN(const std::string& path, std::string& outBody) const
{
    EndpointManager endpoints(GetEndpoints(), config::IsCDNUsingSSL);
    return endpoints.Fetch(path, outBody);
}

std::string LauncherPresenter::FetchFromCDN(const std::string& path) const
{
    std::string body;
    FetchFromCDN(path, body);
    return body;
}

std::filesystem::path LauncherPresenter::GetGamePath() const
{
    char buffer[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

void LauncherPresenter::Initialize() noexcept
{
    LauncherState::SetServerStatus(ServerStatus::Unknown);
    LauncherState::SetStatus(lang::GetString("launcher_waiting_server"));
    LauncherState::SetSpeed("");
    LauncherState::SetProgress(0.0f, 0.0f);
    LauncherState::SetButtons(false, false, false);

    if (!isVerifying_)
    {
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
    else if (!isRunning_.load(std::memory_order_acquire))
    {
        LauncherState::SetButtons(false, true, true);
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
    if (!isWorkerDone_.load(std::memory_order_acquire) || isMaintenance_)
    {
        return;
    }

    if (isRunning_.load(std::memory_order_acquire))
    {
        return;
    }

    if (account.empty() || password.empty())
    {
        LauncherState::SetAuthError(lang::GetString("launcher_login_account"));
        return;
    }

    if (workerThread_.joinable())
    {
        workerThread_.join();
    }

    isRunning_.store(true, std::memory_order_release);
    LauncherState::SetAuthenticating(true);
    LauncherState::SetAuthError("");

    workerThread_ = std::thread([this, account, password, saveAccount]()
    {
        try
        {
            const auto authResponse = AuthClient::Authenticate(account, password);

            if (authResponse.result == AuthClient::AuthResult::Success)
            {
                // Save or clear Remember ID preference
                config::RememberAccount = saveAccount;
                config::SavedAccount = saveAccount ? account : "";
                config::Save(L"");

                LauncherState::SetStatus("Authentication successful!");
                LauncherState::SetAuthenticating(false);
                isRunning_.store(false, std::memory_order_release);

                LaunchGame(account, password);
                shouldClose_ = true;
                return;
            }

            Logger::LogError("Authentication failed: " + authResponse.message);
            LauncherState::SetAuthError(authResponse.message);
            LauncherState::SetStatus(authResponse.message);
        }
        catch (const std::exception& ex)
        {
            Logger::LogError(std::string("Auth thread exception: ") + ex.what());
            LauncherState::SetAuthError("Failed to connect to authentication server.");
            LauncherState::SetStatus("Authentication error");
        }
        catch (...)
        {
            Logger::LogError("Auth thread unknown exception");
            LauncherState::SetAuthError("Unknown authentication error.");
            LauncherState::SetStatus("Authentication error");
        }

        LauncherState::SetAuthenticating(false);
        isRunning_.store(false, std::memory_order_release);
    });
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
    const auto launcherDir = GetGamePath();
    std::filesystem::path optionExe;
    std::error_code ec;

    const std::vector<std::string> candidates = {
        config::OptionExecName,
        "Trickster/Setup.exe",
        "Trickster/apps/Setup.exe",
        "Trickster/Option.exe",
        "Trickster/setup.exe",
        "apps/Setup.exe",
        "Setup.exe"
    };

    for (const auto& candidate : candidates)
    {
        if (candidate.empty()) continue;
        const auto path = launcherDir / candidate;
        if (std::filesystem::exists(path, ec))
        {
            optionExe = path;
            break;
        }
    }

    if (optionExe.empty())
    {
        MessageBoxA(nullptr, lang::GetString("launcher_setup_fail").c_str(), "Error!", MB_OK);
        return;
    }

    const std::string exePath = optionExe.string();
    const std::string workDir = optionExe.has_parent_path() ? optionExe.parent_path().string() : launcherDir.string();

    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, workDir.c_str(), &si, &pi))
    {
        MessageBoxA(nullptr, lang::GetString("launcher_setup_fail").c_str(), "Error!", MB_OK);
        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
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
    options.account = account;
    options.password = password;
    options.gameExecPath = config::GameExecName;
    options.region = config::Region;

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

    // Clean leftover backup/temp files from previous updater runs
    std::error_code ec;
    const std::filesystem::path downloadsDir = launcherPath.parent_path() / "LauncherData" / "downloads";
    std::filesystem::create_directories(downloadsDir, ec); ec.clear();
    std::filesystem::remove(downloadsDir / (launcherName + ".new"), ec); ec.clear();
    std::filesystem::remove(downloadsDir / (launcherName + ".new.part"), ec); ec.clear();
    std::filesystem::remove(downloadsDir / (launcherName + ".new.part.meta"), ec); ec.clear();
    std::filesystem::remove(downloadsDir / (launcherName + ".backup"), ec); ec.clear();
    std::filesystem::remove(downloadsDir / (launcherName + ".bak"), ec); ec.clear();
    std::filesystem::remove(launcherPath.string() + ".backup", ec); ec.clear();
    std::filesystem::remove(launcherPath.string() + ".bak", ec);    ec.clear();
    std::filesystem::remove(launcherPath.string() + ".new.part", ec); ec.clear();
    std::filesystem::remove(launcherPath.string() + ".new.part.meta", ec); ec.clear();

    LauncherUpdater updater(GetEndpoints(), config::IsCDNUsingSSL);
    std::string remoteLauncherHash;
    if (FetchFromCDN("/launcher.txt", remoteLauncherHash))
    {
        if (LauncherState::serverStatus.load(std::memory_order_relaxed) == ServerStatus::Unknown &&
            !isMaintenance_.load(std::memory_order_acquire))
        {
            LauncherState::SetServerStatus(ServerStatus::Online);
            std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
            LauncherState::fileString = lang::GetString("launcher_checking");
        }

        if (!remoteLauncherHash.empty())
        {
            updater.Update(
                launcherPath,
                remoteLauncherHash,
                launcherName,
                currentExe);
        }
    }
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
        try
        {
            // Initial state: waiting for server response
            {
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("launcher_waiting_server");
            }
            LauncherState::SetServerStatus(ServerStatus::Unknown);

            // 1. Check maintenance status from CDN
            std::string maintenanceResponse;
            const bool maintenanceFetched = FetchFromCDN("/maintenance.txt", maintenanceResponse);

            // Trim whitespace
            maintenanceResponse.erase(maintenanceResponse.find_last_not_of(" \n\r\t") + 1);
            maintenanceResponse.erase(0, maintenanceResponse.find_first_not_of(" \n\r\t"));

            const bool maint = (maintenanceResponse == "true");
            isMaintenance_.store(maint, std::memory_order_release);
            LauncherState::SetMaintenance(maint);

            if (maint)
            {
                LauncherState::SetServerStatus(ServerStatus::Maintenance);
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("launcher_worker_maintenance");
                isWorkerDone_.store(true, std::memory_order_release);
                isRunning_.store(false, std::memory_order_release);
                return;
            }

            if (maintenanceFetched)
            {
                LauncherState::SetServerStatus(ServerStatus::Online);
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("launcher_checking");
            }

            // 2. Check self-update
            CheckSelfUpdate();

            // 3. Coordinator check
            {
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("launcher_filelist_building");
            }

            UpdateCoordinator coordinator(
                [this](const std::string& path)
                {
                    std::string body;
                    if (FetchFromCDN(path, body))
                    {
                        if (LauncherState::serverStatus.load(std::memory_order_relaxed) == ServerStatus::Unknown &&
                            !isMaintenance_.load(std::memory_order_acquire))
                        {
                            LauncherState::SetServerStatus(ServerStatus::Online);
                        }
                        return body;
                    }
                    return std::string();
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

            localVersion_ = VersionManager::Load();

            coordinator.Check(
                fileList_,
                isFullCheck,
                localVersion_,
                currentVersion_,
                updateCount_);

            // If no endpoint response was ever received, server status remains Unknown
            if (LauncherState::serverStatus.load(std::memory_order_relaxed) == ServerStatus::Unknown)
            {
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                LauncherState::fileString = lang::GetString("launcher_update_check_fail");
                isWorkerDone_.store(false, std::memory_order_release);
                isRunning_.store(false, std::memory_order_release);
                return;
            }

            const bool success = RunInstaller(fileList_, updateCount_);
            isWorkerDone_.store(success, std::memory_order_release);
            isRunning_.store(false, std::memory_order_release);
            if (success)
            {
                VersionManager::Save(currentVersion_);
            }
            else
            {
                std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
                if (LauncherState::fileString.find("complete") == std::string::npos &&
                    LauncherState::fileString.find("Complete") == std::string::npos)
                {
                    LauncherState::fileString = lang::GetString("launcher_update_download_fail");
                }
            }
        }
        catch (const std::exception& ex)
        {
            Logger::LogError(std::string("Worker thread exception: ") + ex.what());
            isWorkerDone_.store(false, std::memory_order_release);
            isRunning_.store(false, std::memory_order_release);
        }
        catch (...)
        {
            Logger::LogError("Worker thread unknown exception");
            isWorkerDone_.store(false, std::memory_order_release);
            isRunning_.store(false, std::memory_order_release);
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
