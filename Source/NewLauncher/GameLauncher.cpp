#include "GameLauncher.h"

#include <cstring>
#include <thread>

namespace
{
constexpr const char* kGameExecutable = "Trickster.exe";
}

GameLauncher::GameLauncher(Options options)
    : options_(std::move(options))
{
}

std::filesystem::path GameLauncher::ResolveGamePath(const std::filesystem::path& launcherDirectory)
{
    return launcherDirectory / kGameExecutable;
}

bool GameLauncher::InjectDLL(HANDLE process, const std::string& dllPath) const
{
    if (!process || dllPath.empty())
        return false;

    const SIZE_T size = dllPath.size() + 1;
    LPVOID remoteMemory = VirtualAllocEx(process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory)
        return false;

    const BOOL written = WriteProcessMemory(
        process, remoteMemory, dllPath.c_str(), size, nullptr);
    if (!written)
    {
        VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        process,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA),
        remoteMemory,
        0,
        nullptr);
    if (!thread)
    {
        VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
    return true;
}

bool GameLauncher::Launch(const std::filesystem::path& launcherDirectory) const
{
    const std::filesystem::path gamePath = ResolveGamePath(launcherDirectory);
    const std::string executable = gamePath.string();

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    if (!CreateProcessA(
            executable.c_str(),
            nullptr,
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

    bool success = true;
    if (options_.injectDll)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options_.injectionDelayMilliseconds));
        success = InjectDLL(processInfo.hProcess, options_.dllPath);
        if (!success)
            TerminateProcess(processInfo.hProcess, 0);
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return success;
}
