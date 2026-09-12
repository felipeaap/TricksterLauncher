#include "GameLauncher.h"

#include <chrono>
#include <thread>
#include <utility>

namespace
{
constexpr const char* kGameExecutable = "Trickster.exe";
}

GameLauncher::GameLauncher(Options options)
    : options_(std::move(options))
{
}

std::filesystem::path GameLauncher::ResolveGamePath(
    const std::filesystem::path& launcherDirectory,
    const std::string& gameExecPath)
{
    const std::string relativePath = gameExecPath.empty() ? "Trickster/trickster.bin" : gameExecPath;
    return launcherDirectory / relativePath;
}

bool GameLauncher::Launch(const std::filesystem::path& launcherDirectory) const
{
    const std::filesystem::path gamePath = ResolveGamePath(launcherDirectory, options_.gameExecPath);
    const std::string executable = gamePath.string();

    std::string cmdLine = "\"" + executable + "\"";
    if (!options_.commandLineArgs.empty())
    {
        cmdLine += " " + options_.commandLineArgs;
    }
    else if (!options_.account.empty())
    {
        const std::string region = options_.region.empty() ? "thailand" : options_.region;
        cmdLine += " 1," + options_.account + "," + options_.password + ",0," + region + ",|";
    }

    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back('\0');

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    if (!CreateProcessA(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            launcherDirectory.string().c_str(),
            &startupInfo,
            &processInfo))
    {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
