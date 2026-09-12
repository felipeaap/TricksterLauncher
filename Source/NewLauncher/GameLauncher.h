#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

class GameLauncher
{
public:
    struct Options
    {
        std::string commandLineArgs;
        std::string account;
        std::string password;
        std::string gameExecPath;
        std::string region;
    };

    explicit GameLauncher(Options options = {});

    bool Launch(const std::filesystem::path& launcherDirectory) const;

private:
    static std::filesystem::path ResolveGamePath(const std::filesystem::path& launcherDirectory, const std::string& gameExecPath = {});

    Options options_;
};
