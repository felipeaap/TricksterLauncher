#pragma once

#include <windows.h>

class LauncherApplication
{
public:
    int Run(HINSTANCE instance, int commandShow) const;

private:
    static bool RequiresAdmin(const wchar_t* folderPath);
    static void RelaunchAsAdmin(const wchar_t* exePath);
};
