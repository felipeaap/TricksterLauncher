#include "LauncherUpdater.h"

#include <algorithm>
#include <cctype>
#include <windows.h>

#include "Crypt.h"

LauncherUpdater::LauncherUpdater(std::string host, bool useSsl, DownloadManager::Options options)
    : host_(std::move(host)), useSsl_(useSsl), options_(options)
{
}

bool LauncherUpdater::Update(const std::filesystem::path& launcherPath,
                             const std::string& remoteHash,
                             const std::string& remotePath,
                             const std::string& currentExecutable) const
{
    if (remoteHash.empty() || currentExecutable.empty())
        return false;

    const std::string localHash = crypt::createMD5FromFile(launcherPath.string());
    if (localHash.empty() || localHash == remoteHash)
        return true;

    DownloadManager manager(host_, useSsl_, options_);
    if (!manager.Download(remotePath, launcherPath.string()))
        return false;

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    const bool launched = CreateProcessA(
        currentExecutable.c_str(),
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo) != FALSE;

    if (!launched)
        return false;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    ExitProcess(0);
    return true;
}
