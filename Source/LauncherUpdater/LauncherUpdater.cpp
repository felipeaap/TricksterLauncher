#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>

#include "../NewLauncher/Integrity.h"

namespace
{
std::string GetArg(int argc, char** argv, const char* name)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == name)
            return argv[i + 1];
    return {};
}

bool WaitForProcess(DWORD pid)
{
    if (!pid)
        return true;
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process)
        return GetLastError() == ERROR_INVALID_PARAMETER;
    const DWORD result = WaitForSingleObject(process, 30000);
    CloseHandle(process);
    return result == WAIT_OBJECT_0;
}

bool Launch(const std::filesystem::path& path)
{
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const std::string command = "\"" + path.string() + "\"";
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, path.parent_path().string().c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
}

int main(int argc, char** argv)
{
    const std::string targetArg = GetArg(argc, argv, "--target");
    const std::string newArg = GetArg(argc, argv, "--new");
    const std::string backupArg = GetArg(argc, argv, "--backup");
    const std::string hashArg = GetArg(argc, argv, "--hash");
    const std::string pidArg = GetArg(argc, argv, "--pid");
    if (targetArg.empty() || newArg.empty() || backupArg.empty() || hashArg.empty())
        return 2;
    if (!pidArg.empty())
        WaitForProcess(static_cast<DWORD>(std::stoul(pidArg)));

    const std::filesystem::path target(targetArg);
    const std::filesystem::path replacement(newArg);
    const std::filesystem::path backup(backupArg);
    if (integrity::createHashFromFile(replacement.string(), hashArg) != hashArg)
        return 3;

    std::error_code error;
    if (std::filesystem::exists(backup, error))
        std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(target, error) &&
        !MoveFileExA(target.string().c_str(), backup.string().c_str(), MOVEFILE_WRITE_THROUGH))
        return 4;
    error.clear();
    if (!MoveFileExA(replacement.string().c_str(), target.string().c_str(), MOVEFILE_WRITE_THROUGH))
    {
        MoveFileExA(backup.string().c_str(), target.string().c_str(), MOVEFILE_WRITE_THROUGH);
        return 5;
    }
    if (!Launch(target))
    {
        MoveFileExA(target.string().c_str(), replacement.string().c_str(), MOVEFILE_WRITE_THROUGH);
        MoveFileExA(backup.string().c_str(), target.string().c_str(), MOVEFILE_WRITE_THROUGH);
        return 6;
    }
    std::filesystem::remove(backup, error);
    return 0;
}
