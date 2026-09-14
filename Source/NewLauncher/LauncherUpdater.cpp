#include "LauncherUpdater.h"

#include <utility>
#include <windows.h>

#include "EndpointManager.h"
#include "Integrity.h"

LauncherUpdater::LauncherUpdater(std::vector<std::string> hosts,
                                 bool useSsl,
                                 DownloadManager::Options options)
    : hosts_(std::move(hosts)), useSsl_(useSsl), options_(options)
{
}

bool LauncherUpdater::Update(const std::filesystem::path& launcherPath,
                             const std::string& remoteHash,
                             const std::string& remotePath,
                             const std::string& currentExecutable) const
{
    if (remoteHash.empty() || currentExecutable.empty())
        return false;

    // Fast-path: Check if local executable matches cached timestamp & size
    static std::string s_cachedPath;
    static std::string s_cachedHash;
    static unsigned long long s_cachedMtime = 0;
    static long long s_cachedSize = 0;

    std::string localHash;
    WIN32_FILE_ATTRIBUTE_DATA attrData{};
    if (GetFileAttributesExW(launcherPath.c_str(), GetFileExInfoStandard, &attrData))
    {
        ULARGE_INTEGER ft{ attrData.ftLastWriteTime.dwLowDateTime, attrData.ftLastWriteTime.dwHighDateTime };
        ULARGE_INTEGER sz{ attrData.nFileSizeLow, attrData.nFileSizeHigh };
        if (s_cachedPath == launcherPath.string() &&
            s_cachedMtime == ft.QuadPart &&
            s_cachedSize == static_cast<long long>(sz.QuadPart) &&
            !s_cachedHash.empty())
        {
            localHash = s_cachedHash;
        }
        else
        {
            localHash = integrity::createHashFromFile(launcherPath.string(), remoteHash);
            if (!localHash.empty())
            {
                s_cachedPath = launcherPath.string();
                s_cachedMtime = ft.QuadPart;
                s_cachedSize = static_cast<long long>(sz.QuadPart);
                s_cachedHash = localHash;
            }
        }
    }
    else
    {
        localHash = integrity::createHashFromFile(launcherPath.string(), remoteHash);
    }

    if (localHash.empty() || localHash == remoteHash)
        return true;

    const std::filesystem::path downloadsDir = launcherPath.parent_path() / "LauncherData" / "downloads";
    std::error_code error;
    std::filesystem::create_directories(downloadsDir, error);

    const std::filesystem::path stagedPath = downloadsDir / (launcherPath.filename().string() + ".new");
    std::filesystem::path updaterPath = launcherPath.parent_path() / "apps" / "LauncherUpdater.exe";
    if (!std::filesystem::exists(updaterPath, error))
    {
        const auto rootUpdater = launcherPath.parent_path() / "LauncherUpdater.exe";
        if (std::filesystem::exists(rootUpdater, error))
            updaterPath = rootUpdater;
    }
    const std::filesystem::path backupPath = downloadsDir / (launcherPath.filename().string() + ".backup");

    std::filesystem::remove(stagedPath, error);
    std::filesystem::remove(stagedPath.string() + ".part", error);
    std::filesystem::remove(stagedPath.string() + ".part.meta", error);

    EndpointManager endpoints(hosts_, useSsl_, options_);
    if (!endpoints.Download(remotePath, stagedPath.string()))
        return false;

    const std::string downloadedHash = integrity::createHashFromFile(stagedPath.string(), remoteHash);
    if (downloadedHash.empty() ||
        downloadedHash != remoteHash ||
        !std::filesystem::exists(updaterPath, error))
    {
        std::filesystem::remove(stagedPath, error);
        return false;
    }

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    std::string command = "\"" + updaterPath.string() + "\""
        " --target \"" + launcherPath.string() + "\""
        " --new \"" + stagedPath.string() + "\""
        " --backup \"" + backupPath.string() + "\""
        " --hash \"" + remoteHash + "\""
        " --pid " + std::to_string(GetCurrentProcessId());
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    if (!CreateProcessA(nullptr,
                        mutableCommand.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        launcherPath.parent_path().string().c_str(),
                        &startupInfo,
                        &processInfo))
    {
        std::filesystem::remove(stagedPath, error);
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    ExitProcess(0);
    return true;
}
