#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

class GameLauncher
{
public:
    struct Options
    {
        bool injectDll = false;
        std::string dllPath;
        DWORD injectionDelayMilliseconds = 2000;
    };

    explicit GameLauncher(Options options = {});

    bool Launch(const std::filesystem::path& launcherDirectory) const;

private:
    static std::filesystem::path ResolveGamePath(const std::filesystem::path& launcherDirectory);
    bool InjectDLL(HANDLE process, const std::string& dllPath) const;

    Options options_;
};
