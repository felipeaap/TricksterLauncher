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

    const std::string localHash = integrity::createHashFromFile(launcherPath.string(), remoteHash);
    if (localHash.empty() || localHash == remoteHash)
        return true;

    EndpointManager endpoints(hosts_, useSsl_, options_);
    if (!endpoints.Download(remotePath, launcherPath.string()))
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
