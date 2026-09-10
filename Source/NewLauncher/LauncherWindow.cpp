#include "LauncherWindow.h"

#include "Gui.h"
#include "imgui_impl_win32.h"

namespace
{
constexpr wchar_t kWindowClassName[] = L"class001";
constexpr int kWebViewLeft = 19;
constexpr int kWebViewTop = 32;
constexpr int kWebViewWidth = 503;
constexpr int kWebViewHeight = 343;

LRESULT CALLBACK WindowProcess(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter)
{
    if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
        return true;

    switch (message)
    {
    case WM_SIZE:
        if (gui::device && wideParameter != SIZE_MINIMIZED)
        {
            gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
            gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
            gui::ResetDevice();
        }
        return 0;

    case WM_MOVE:
        if (gui::g_controller)
        {
            RECT bounds = {
                kWebViewLeft,
                kWebViewTop,
                kWebViewLeft + kWebViewWidth,
                kWebViewTop + kWebViewHeight
            };
            gui::g_controller->put_Bounds(bounds);
        }
        break;

    case WM_SYSCOMMAND:
        if ((wideParameter & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        gui::isRunning = false;
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
        gui::position = MAKEPOINTS(longParameter);
        return 0;

    case WM_MOUSEMOVE:
        if (wideParameter == MK_LBUTTON)
        {
            const POINTS points = MAKEPOINTS(longParameter);
            RECT rect{};
            GetWindowRect(gui::window, &rect);
            rect.left += points.x - gui::position.x;
            rect.top += points.y - gui::position.y;

            if (gui::position.x >= 0 && gui::position.x <= gui::WIDTH &&
                gui::position.y >= 0 && gui::position.y <= 19)
            {
                SetWindowPos(
                    gui::window,
                    HWND_TOPMOST,
                    rect.left,
                    rect.top,
                    0,
                    0,
                    SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        return 0;
    }

    return DefWindowProc(window, message, wideParameter, longParameter);
}
}

bool LauncherWindow::Create(const wchar_t* title) const noexcept
{
    if (!title)
        return false;

    gui::windowClass.cbSize = sizeof(WNDCLASSEXW);
    gui::windowClass.style = CS_CLASSDC;
    gui::windowClass.lpfnWndProc = WindowProcess;
    gui::windowClass.cbClsExtra = 0;
    gui::windowClass.cbWndExtra = 0;
    gui::windowClass.hInstance = GetModuleHandleW(nullptr);
    gui::windowClass.hIcon = nullptr;
    gui::windowClass.hCursor = nullptr;
    gui::windowClass.hbrBackground = nullptr;
    gui::windowClass.lpszMenuName = nullptr;
    gui::windowClass.lpszClassName = kWindowClassName;
    gui::windowClass.hIconSm = nullptr;

    if (!RegisterClassExW(&gui::windowClass))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    gui::window = CreateWindowExW(
        0,
        kWindowClassName,
        title,
        WS_POPUP,
        100,
        100,
        gui::WIDTH,
        gui::HEIGHT,
        nullptr,
        nullptr,
        gui::windowClass.hInstance,
        nullptr);

    if (!gui::window)
    {
        UnregisterClassW(kWindowClassName, gui::windowClass.hInstance);
        return false;
    }

    ShowWindow(gui::window, SW_SHOWDEFAULT);
    UpdateWindow(gui::window);
    return true;
}

void LauncherWindow::Destroy() const noexcept
{
    if (gui::window)
    {
        DestroyWindow(gui::window);
        gui::window = nullptr;
    }

    if (gui::windowClass.hInstance)
        UnregisterClassW(kWindowClassName, gui::windowClass.hInstance);
}

bool LauncherWindow::PumpMessages() const noexcept
{
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            gui::isRunning = false;
            return false;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return gui::isRunning;
}
