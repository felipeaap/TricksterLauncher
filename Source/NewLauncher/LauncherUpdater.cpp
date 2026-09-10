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

    const std::filesystem::path stagedPath = launcherPath.string() + ".new";
    const std::filesystem::path updaterPath = launcherPath.parent_path() / "LauncherUpdater.exe";
    const std::filesystem::path backupPath = launcherPath.string() + ".backup";

    std::error_code error;
    std::filesystem::remove(stagedPath, error);
    std::filesystem::remove(stagedPath.string() + ".part", error);
    std::filesystem::remove(stagedPath.string() + ".part.meta", error);

    EndpointManager endpoints(hosts_, useSsl_, options_);
    if (!endpoints.Download(remotePath, stagedPath.string()))
        return false;

    const std::string downloadedHash = integrity::createSHA256FromFile(stagedPath.string());
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
        " --sha256 \"" + remoteHash + "\""
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
