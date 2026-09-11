#include "Helper.h"

#include "Config.h"
#include "EndpointManager.h"
#include "FileVerifier.h"
#include "GameLauncher.h"
#include "LauncherState.h"
#include "Language.h"
#include "LauncherUpdater.h"
#include "ManifestManager.h"
#include "UpdateCoordinator.h"
#include "UpdateInstaller.h"
#include "VersionManager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <windows.h>

namespace
{
std::vector<std::string> GetEndpoints()
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
}

Helper::Helper() = default;

void Helper::GetLocalVersion()
{
    localVersion = VersionManager::Load();
}

void Helper::SaveLocalVersion(int version)
{
    VersionManager::Save(version);
}

void Helper::ParseVersionedFileLists(bool isFullCheck)
{
    if (!isFullCheck)
        GetLocalVersion();

    std::lock_guard<std::mutex> lockFile(LauncherState::fileStringMutex);
    LauncherState::fileString = lang::GetString("launcher_filelist_building");

    ManifestManager manifests([this](const std::string& path)
    {
        return GetFileFromURL(path);
    });

    currentVersion = manifests.Load(ListaArquivos, isFullCheck, localVersion);
}

std::string Helper::GetFileFromURL(int iType)
{
    static const std::unordered_map<int, std::string> paths = {
        {0, "/maintenance.txt"},
        {1, "/launcher.txt"}
    };

    const auto it = paths.find(iType);
    return GetFileFromURL(it != paths.end() ? it->second : "/maintenance.txt");
}

std::string Helper::GetFileFromURL(const std::string& customPath)
{
    EndpointManager endpoints(GetEndpoints(), config::IsCDNUsingSSL);
    return endpoints.Get(customPath);
}

bool Helper::iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }

    return true;
}

void Helper::FileCheckUpdate()
{
    LauncherState::fileProgress.store(1.0f, std::memory_order_relaxed);

    auto progressCallback = [&](size_t current, size_t total, const Arquivo& file)
    {
        float launcherPercent = total > 0
            ? static_cast<float>(current) / static_cast<float>(total)
            : 1.0f;
        launcherPercent = std::min(launcherPercent, 1.0f);

        const std::string fileName =
            file.FilePath.substr(file.FilePath.find_last_of("/\\") + 1);

        LauncherState::totalProgress.store(launcherPercent, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(LauncherState::fileStringMutex);
            LauncherState::fileString = lang::GetString("splash_check") + fileName;
        }
    };
    FileVerifier verifier(std::move(progressCallback));

    updateCount = verifier.CountUpdates(ListaArquivos);
    const bool success = WorkerUpdating(updateCount);
    isWorkerDone.store(success, std::memory_order_release);
    if (success)
        SaveLocalVersion(currentVersion);
}

void Helper::CheckWorker(bool isFullCheck)
{
    if (!ListaArquivos.empty() && !isWorkerDone.load(std::memory_order_acquire))
        return;

    if (workerThread.joinable())
        workerThread.join();

    isWorkerDone.store(false, std::memory_order_release);
    workerThread = std::thread([this, isFullCheck]()
    {
        UpdateCoordinator coordinator(
            [this](const std::string& path)
            {
                return GetFileFromURL(path);
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

        coordinator.Check(
            ListaArquivos,
            isFullCheck,
            localVersion,
            currentVersion,
            updateCount);

        const bool success = WorkerUpdating(updateCount);
        isWorkerDone.store(success, std::memory_order_release);
        if (success)
            SaveLocalVersion(currentVersion);
    });
}

bool Helper::WorkerUpdating(int pendingUpdateCount)
{
    LauncherState::totalProgress.store(0.0f, std::memory_order_relaxed);

    UpdateInstaller installer(GetEndpoints(), config::IsCDNUsingSSL);
    const bool result = installer.Install(
        ListaArquivos,
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
        LauncherState::fileString = isMaintenance
            ? lang::GetString("launcher_worker_maintenance")
            : lang::GetString("launcher_worker_complete");
    }

    return true;
}

bool Helper::InjectDLL(HANDLE hProcess, const std::string& dllPath)
{
    GameLauncher launcher({ true, dllPath, 0 });
    return launcher.InjectDLL(hProcess, dllPath);
}

std::filesystem::path Helper::GetGamePath()
{
    char buffer[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

void Helper::ClickPlayButton()
{
    GameLauncher::Options options;
    options.injectDll = config::IsDllInjectEnable;
    options.dllPath = config::InjectDLLName;
    options.injectionDelayMilliseconds = 2000;

    GameLauncher launcher(options);
    if (launcher.Launch(GetGamePath()))
        ExitProcess(0);
}

void Helper::UpdateLauncher()
{
    const std::string launcherName = "Splash.exe";

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    const std::string currentExe = exePath;
    const std::filesystem::path launcherPath(currentExe);

    LauncherUpdater updater(GetEndpoints(), config::IsCDNUsingSSL);
    const std::string remoteLauncherHash = GetFileFromURL(1);
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
