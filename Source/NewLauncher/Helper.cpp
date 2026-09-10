#include "Helper.h"
#include "Gui.h"
#include "Config.h"
#include "Language.h"
#include "VersionManager.h"
#include "ManifestManager.h"
#include "FileVerifier.h"
#include "DownloadManager.h"
#include "EndpointManager.h"
#include "UpdateCoordinator.h"
#include "GameLauncher.h"
#include "LauncherUpdater.h"
#include <direct.h>
#include <Shlwapi.h>
#include <algorithm>
#include <cctype>

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

Helper::Helper()
{
    isMaintenance = false;
    isWorkerDone = false;
    g_PopupPage = 0;
    updateCount = 0;
    ListaArquivos.clear();
    localVersion = 0;
    currentVersion = 0;
}

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

    std::lock_guard<std::mutex> lockFile(gui::g_FileStringMutex);
    gui::g_FileString = lang::GetString("launcher_filelist_building");

    ManifestManager manifests([this](const std::string& path) {
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
    std::string path = paths.count(iType) ? paths.at(iType) : "/maintenance.txt";
    return GetFileFromURL(path);
}

std::string Helper::GetFileFromURL(const std::string& customPath)
{
    EndpointManager endpoints(GetEndpoints(), config::IsCDNUsingSSL);
    return endpoints.Get(customPath);
}

bool Helper::iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

void Helper::FileCheckUpdate()
{
    gui::g_iFileProgress.store(1.0, std::memory_order_relaxed);

    FileVerifier verifier([&](size_t current, size_t total, const Arquivo& file)
    {
        float launcherPercent = total > 0
            ? static_cast<float>(current) / static_cast<float>(total)
            : 1.0f;
        launcherPercent = std::min(launcherPercent, 1.0f);

        std::string fileName = file.FilePath.substr(file.FilePath.find_last_of("/\\") + 1);
        gui::g_iTotalProgress.store(launcherPercent, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(gui::g_FileStringMutex);
            gui::g_FileString = lang::GetString("splash_check") + fileName;
        }
    });

    updateCount = verifier.CountUpdates(ListaArquivos);
    WorkerUpdating(updateCount);
    isWorkerDone = true;
    SaveLocalVersion(currentVersion);
}

void Helper::CheckWorker(bool isFullCheck)
{
    if (!ListaArquivos.empty() && !isWorkerDone)
        return;

    if (workerThread.joinable())
        workerThread.join();

    isWorkerDone = false;
    workerThread = std::thread([this, isFullCheck]()
    {
        UpdateCoordinator coordinator(
            [this](const std::string& path) {
                return GetFileFromURL(path);
            },
            [](const std::string& fileName)
            {
                std::lock_guard<std::mutex> lock(gui::g_FileStringMutex);
                gui::g_FileString = lang::GetString("splash_check") + fileName;
            },
            [](float fileProgress, float totalProgress)
            {
                gui::g_iFileProgress.store(fileProgress, std::memory_order_relaxed);
                gui::g_iTotalProgress.store(totalProgress, std::memory_order_relaxed);
            });

        {
            std::lock_guard<std::mutex> lock(gui::g_FileStringMutex);
            gui::g_FileString = lang::GetString("launcher_filelist_building");
        }

        coordinator.Check(ListaArquivos, isFullCheck, localVersion, currentVersion, updateCount);
        WorkerUpdating(updateCount);
        isWorkerDone = true;
        SaveLocalVersion(currentVersion);
    });
}

std::string Helper::GetDirectoryFromPath(const std::string& filepath)
{
    size_t pos = filepath.find_last_of("/\\");
    return (pos != std::string::npos) ? filepath.substr(0, pos) : "";
}

bool Helper::CreateDirectoryIfNotExists(const std::string& dirPath)
{
    std::string fixedDirPath = dirPath;
    std::replace(fixedDirPath.begin(), fixedDirPath.end(), '\\', '/');
    std::istringstream iss(fixedDirPath);
    std::string token;
    std::string path;
    while (std::getline(iss, token, '/'))
    {
        if (token.empty()) continue;
        path += token + "/";
        _mkdir(path.c_str());
    }
    return true;
}

bool Helper::DownloadFile(const std::string& remoteFile, const std::string& localPath, int fileIndex, int totalFiles)
{
    EndpointManager endpoints(GetEndpoints(), config::IsCDNUsingSSL);
    const bool result = endpoints.Download(
        remoteFile,
        localPath,
        [&](long long downloaded, long long contentLength)
        {
            if (contentLength > 0)
                gui::g_iFileProgress.store(
                    static_cast<float>(downloaded) / static_cast<float>(contentLength),
                    std::memory_order_relaxed);

            gui::g_iTotalProgress.store(
                totalFiles > 0
                    ? (static_cast<float>(fileIndex - 1) + gui::g_iFileProgress.load()) / totalFiles
                    : 1.0f,
                std::memory_order_relaxed);
        },
        [&](double bytesPerSecond)
        {
            double speed = bytesPerSecond;
            const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };
            int unit = 0;
            while (speed >= 1024.0 && unit < 4)
            {
                speed /= 1024.0;
                ++unit;
            }

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << speed << " " << units[unit];
            std::lock_guard<std::mutex> lock(gui::g_SpeedStringMutex);
            gui::g_SpeedString = oss.str();
        });

    {
        std::lock_guard<std::mutex> lock(gui::g_SpeedStringMutex);
        gui::g_SpeedString.clear();
    }

    return result;
}

void Helper::WorkerUpdating(int updateCount)
{
    int index = 1;
    gui::g_iTotalProgress.store(0.0f, std::memory_order_relaxed);
    if (updateCount > 0)
    {
        for (const auto& file : ListaArquivos)
        {
            if (!file.ToUpdate) continue;
            gui::g_iFileProgress = 0.0f;
            std::string fileName = file.FilePath.substr(file.FilePath.find_last_of("/\\") + 1);
            std::string remotePath = file.FilePath;
            std::replace(remotePath.begin(), remotePath.end(), '\\', '/');
            std::string dirPath = GetDirectoryFromPath(file.FilePath);
            CreateDirectoryIfNotExists(dirPath);
            {
                std::lock_guard<std::mutex> lockFile(gui::g_FileStringMutex);
                gui::g_FileString = lang::GetString("launcher_worker_downloading") + ": " + fileName;
            }
            DownloadFile(remotePath, file.FilePath, index, updateCount);
            index++;
        }
    }
    gui::g_iFileProgress.store(1.0f, std::memory_order_relaxed);
    gui::g_iTotalProgress.store(1.0f, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lockFile(gui::g_FileStringMutex);
        if (!isMaintenance)
            gui::g_FileString = lang::GetString("launcher_worker_complete");
        else
            gui::g_FileString = lang::GetString("launcher_worker_maintenance");
    }
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
    std::string launcherName = "Splash.exe";
    std::transform(launcherName.begin(), launcherName.end(), launcherName.begin(), [](unsigned char c) { return std::tolower(c); });

    std::string currentExe;
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    currentExe = exePath;

    LauncherUpdater updater(GetEndpoints(), config::IsCDNUsingSSL);
    const std::string remoteLauncherHash = GetFileFromURL(1);
    if (remoteLauncherHash.empty())
    {
        MessageBoxA(NULL, lang::GetString("launcher_update_check_fail").c_str(), "Error!", MB_OK);
        PostQuitMessage(0);
        return;
    }

    if (!updater.Update(launcherName, remoteLauncherHash, launcherName, currentExe))
    {
        MessageBoxA(NULL, lang::GetString("launcher_update_download_fail").c_str(), "Error!", MB_OK);
        PostQuitMessage(0);
    }
}
