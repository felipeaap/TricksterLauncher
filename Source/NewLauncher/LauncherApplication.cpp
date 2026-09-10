#include "LauncherApplication.h"

#include <chrono>
#include <filesystem>
#include <shellapi.h>
#include <thread>

#include <shlwapi.h>

#include "Config.h"
#include "Gui.h"

bool LauncherApplication::RequiresAdmin(const wchar_t* folderPath)
{
    if (!folderPath || !*folderPath)
        return false;

    const std::wstring testFile = std::wstring(folderPath) + L"\\perm_test.tmp";
    HANDLE file = CreateFileW(
        testFile.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr);

    if (file == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_ACCESS_DENIED;

    CloseHandle(file);
    return false;
}

void LauncherApplication::RelaunchAsAdmin(const wchar_t* exePath)
{
    if (!exePath || !*exePath)
        return;

    SHELLEXECUTEINFOW shellExecute{};
    shellExecute.cbSize = sizeof(shellExecute);
    shellExecute.lpVerb = L"runas";
    shellExecute.lpFile = exePath;
    shellExecute.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&shellExecute) && GetLastError() == ERROR_CANCELLED)
        MessageBoxW(nullptr, L"Administrator access is necessary!", L"Warning!", MB_ICONWARNING);
}

int LauncherApplication::Run(HINSTANCE instance, int commandShow) const
{
    (void)instance;
    (void)commandShow;

    wchar_t exePath[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return EXIT_FAILURE;

    wchar_t folderPath[MAX_PATH]{};
    wcscpy_s(folderPath, exePath);
    PathRemoveFileSpecW(folderPath);

    if (RequiresAdmin(folderPath))
    {
        RelaunchAsAdmin(exePath);
        return EXIT_SUCCESS;
    }

    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return EXIT_FAILURE;

    gui::CreateHWindow(config::WindowTitle.c_str());
    if (!gui::CreateDevice())
    {
        gui::DestroyHWindow();
        CoUninitialize();
        return EXIT_FAILURE;
    }

    gui::CreateImGui();
    gui::LoadResources();
    gui::InitWebView(gui::window);

    MSG msg{};
    while (gui::isRunning)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                gui::isRunning = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        gui::BeginRender();
        gui::Render();
        gui::EndRender();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    gui::DestroyImGui();
    gui::DestroyDevice();
    gui::DestroyHWindow();
    CoUninitialize();
    return EXIT_SUCCESS;
}
