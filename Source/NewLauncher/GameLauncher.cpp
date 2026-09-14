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
    std::error_code ec;
    if (!gameExecPath.empty())
    {
        const auto direct = launcherDirectory / gameExecPath;
        if (std::filesystem::exists(direct, ec))
            return direct;
    }

    const std::vector<std::string> candidates = {
        "Trickster/trickster.bin",
        "Trickster/Trickster.bin",
        "Trickster/Game.exe",
        "Trickster/game.exe",
        "Trickster/Trickster.exe",
        "trickster.bin",
        "Game.exe",
        "Trickster.exe"
    };

    for (const auto& candidate : candidates)
    {
        const auto path = launcherDirectory / candidate;
        if (std::filesystem::exists(path, ec))
            return path;
    }

    return launcherDirectory / (gameExecPath.empty() ? "Trickster/trickster.bin" : gameExecPath);
}

bool GameLauncher::Launch(const std::filesystem::path& launcherDirectory) const
{
    const std::filesystem::path gamePath = ResolveGamePath(launcherDirectory, options_.gameExecPath);
    const std::string executable = gamePath.string();
    const std::string workingDirectory = gamePath.has_parent_path() ? gamePath.parent_path().string() : launcherDirectory.string();

    std::string cmdLine = "\"" + executable + "\"";
    if (!options_.commandLineArgs.empty())
    {
        cmdLine += " " + options_.commandLineArgs;
    }
    else if (!options_.account.empty())
    {
        const std::string region = options_.region.empty() ? "thailand" : options_.region;
        cmdLine += " " + options_.account + "," + options_.account + "," + options_.password + ",Trickster,0," + region + ",|";
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
            workingDirectory.c_str(),
            &startupInfo,
            &processInfo))
    {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
