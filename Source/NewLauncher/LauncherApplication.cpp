#include "LauncherApplication.h"

#include <chrono>
#include <filesystem>
#include <shellapi.h>
#include <shlwapi.h>
#include <thread>

#include "Config.h"
#include "Language.h"
#include "LauncherPresenter.h"
#include "LauncherState.h"
#include "LauncherView.h"
#include "LauncherWebView.h"
#include "LauncherWindow.h"
#include "Logger.h"
#include "RendererD3D9.h"
#include "TextureManager.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

bool LauncherApplication::RequiresAdmin(const wchar_t* folderPath)
{
    if (!folderPath || !*folderPath)
        return false;

    const std::wstring testFile = std::wstring(folderPath) + L"\\perm_test.tmp";
    HANDLE file = CreateFileW(
        testFile.c_str(),
        GENERIC_WRITE, 0, nullptr,
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
    shellExecute.nShow  = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&shellExecute) && GetLastError() == ERROR_CANCELLED)
        MessageBoxW(nullptr, L"Administrator access is necessary!", L"Warning!", MB_ICONWARNING);
}

int LauncherApplication::Run(HINSTANCE instance, int commandShow) const
{
    (void)commandShow;

    // ── Resolve exe path & directory ────────────────────────────────────────
    wchar_t exePath[MAX_PATH]{};
    if (!GetModuleFileNameW(instance, exePath, MAX_PATH))
        return EXIT_FAILURE;

    wchar_t folderPath[MAX_PATH]{};
    wcscpy_s(folderPath, exePath);
    PathRemoveFileSpecW(folderPath);

    // ── Logger ──────────────────────────────────────────────────────────────
    {
        const std::filesystem::path logPath =
            std::filesystem::path(folderPath) / L"launcher_error.log";
        Logger::Init(logPath.string());
    }
    Logger::Log("LauncherApplication::Run started");

    // ── Config.json ─────────────────────────────────────────────────────────
    config::Load(folderPath);
    Logger::Log("Config loaded (cdn=" + config::LauncherCDN + ")");

    // ── Admin check ─────────────────────────────────────────────────────────
    if (RequiresAdmin(folderPath))
    {
        Logger::Log("RequiresAdmin -> RelaunchAsAdmin");
        RelaunchAsAdmin(exePath);
        Logger::Shutdown();
        return EXIT_SUCCESS;
    }

    // ── COM init ─────────────────────────────────────────────────────────────
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        Logger::LogError("CoInitializeEx failed hr=" + std::to_string(hr));
        Logger::Shutdown();
        return EXIT_FAILURE;
    }

    // ── Application objects ──────────────────────────────────────────────────
    LauncherWindow   launcherWindow;
    RendererD3D9     renderer;
    LauncherWebView  webView;
    TextureManager   textureManager;
    LauncherView     launcherView;
    LauncherPresenter presenter;

    // ── Window resize / move callbacks ────────────────────────────────────────
    launcherWindow.SetResizeCallback([&renderer](UINT width, UINT height)
    {
        if (renderer.Device())
        {
            renderer.SetBackBufferSize(width, height);
            renderer.Reset();
        }
    });

    launcherWindow.SetMoveCallback([&webView]()
    {
        const RECT bounds{ 19, 40, 19 + 501, 40 + 326 };
        webView.SetBounds(bounds);
    });

    // ── Create window ────────────────────────────────────────────────────────
    if (!launcherWindow.Create(config::WindowTitle.c_str()))
    {
        Logger::LogError("LauncherWindow::Create failed");
        CoUninitialize();
        Logger::Shutdown();
        return EXIT_FAILURE;
    }
    Logger::Log("LauncherWindow created");

    // ── Create D3D9 device ───────────────────────────────────────────────────
    if (!renderer.Create(launcherWindow.Handle()))
    {
        Logger::LogError("RendererD3D9::Create failed");
        launcherWindow.Destroy();
        CoUninitialize();
        Logger::Shutdown();
        return EXIT_FAILURE;
    }
    Logger::Log("D3D9 device created");

    // ── Wire device-reset callbacks for TextureManager (P1 – Device Lost) ────
    renderer.SetDeviceResetCallbacks(
        [&textureManager]()
        {
            Logger::Log("Device lost — releasing textures");
            textureManager.OnDeviceLost();
        },
        [&textureManager, &renderer, instance]()
        {
            Logger::Log("Device reset — reloading textures");
            textureManager.OnDeviceReset(renderer.Device(), instance);
        }
    );

    // ── ImGui ────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    launcherView.Initialize();

    ImGui_ImplWin32_Init(launcherWindow.Handle());
    ImGui_ImplDX9_Init(renderer.Device());
    Logger::Log("ImGui initialised");

    // ── Resources & presenter ────────────────────────────────────────────────
    textureManager.LoadAll(renderer.Device(), instance);
    Logger::Log("Textures loaded");

    presenter.Initialize();
    Logger::Log("Presenter initialised");

    LauncherViewEvents viewEvents = presenter.CreateViewEvents();
    viewEvents.onMinimizeClicked = [&launcherWindow]()
    {
        launcherWindow.Minimize();
    };

    // ── WebView2 ─────────────────────────────────────────────────────────────
    webView.Initialize(launcherWindow.Handle(), config::BaseNewsURL);
    Logger::Log("WebView2 initialised");

    // ── Main render loop ─────────────────────────────────────────────────────
    Logger::Log("Entering render loop");
    while (launcherWindow.PumpMessages() &&
           !presenter.ShouldClose()       &&
           !launcherView.ShouldClose())
    {
        presenter.Update();

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const LauncherViewState state = LauncherState::GetSnapshot();
        launcherView.Render(state, textureManager, viewEvents);

        ImGui::EndFrame();
        if (renderer.BeginFrame())
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
        renderer.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Logger::Log("Render loop exited");

    // ── Teardown ─────────────────────────────────────────────────────────────
    webView.Shutdown();
    presenter.Shutdown();
    textureManager.ReleaseAll();

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    renderer.Destroy();
    launcherWindow.Destroy();
    CoUninitialize();

    Logger::Log("LauncherApplication::Run finished");
    Logger::Shutdown();
    return EXIT_SUCCESS;
}
