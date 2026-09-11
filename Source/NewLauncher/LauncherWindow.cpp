#include "LauncherWindow.h"

#include "Gui.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
constexpr int kWebViewLeft = 19;
constexpr int kWebViewTop = 32;
constexpr int kWebViewWidth = 503;
constexpr int kWebViewHeight = 343;
}

LauncherWindow::~LauncherWindow()
{
    Destroy();
}

bool LauncherWindow::Create(const wchar_t* title) noexcept
{
    if (!title || window_)
        return false;

    instance_ = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = &LauncherWindow::WindowProcess;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&windowClass))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }
    else
    {
        classRegistered_ = true;
    }

    window_ = CreateWindowExW(
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
        instance_,
        this);

    if (!window_)
    {
        if (classRegistered_)
        {
            UnregisterClassW(kWindowClassName, instance_);
            classRegistered_ = false;
        }
        return false;
    }

    running_ = true;
    ShowWindow(window_, SW_SHOWDEFAULT);
    UpdateWindow(window_);

    // Associate window with GUI subsystem
    gui::SetWindow(window_);
    gui::SetRunning(true);

    return true;
}

void LauncherWindow::Destroy() noexcept
{
    const HWND destroyedWindow = window_;

    if (window_)
    {
        DestroyWindow(window_);
        window_ = nullptr;
    }

    if (classRegistered_ && instance_)
    {
        UnregisterClassW(kWindowClassName, instance_);
        classRegistered_ = false;
    }

    if (gui::GetWindow() == destroyedWindow)
        gui::SetWindow(nullptr);

    instance_ = nullptr;
    running_ = false;
}

bool LauncherWindow::PumpMessages() noexcept
{
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            running_ = false;
            gui::SetRunning(false);
            return false;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return running_ && gui::IsRunning();
}

LRESULT CALLBACK LauncherWindow::WindowProcess(
    HWND window,
    UINT message,
    WPARAM wideParameter,
    LPARAM longParameter)
{
    auto* self = reinterpret_cast<LauncherWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(longParameter);
        self = static_cast<LauncherWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
        return TRUE;

    if (self)
        return self->HandleMessage(message, wideParameter, longParameter);

    return DefWindowProcW(window, message, wideParameter, longParameter);
}

LRESULT LauncherWindow::HandleMessage(UINT message, WPARAM wideParameter, LPARAM longParameter) noexcept
{
    switch (message)
    {
    case WM_SIZE:
        if (wideParameter != SIZE_MINIMIZED && resizeCallback_)
            resizeCallback_(LOWORD(longParameter), HIWORD(longParameter));
        return 0;

    case WM_MOVE:
        if (moveCallback_)
            moveCallback_();
        break;

    case WM_SYSCOMMAND:
        if ((wideParameter & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        running_ = false;
        gui::SetRunning(false);
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:
        dragOrigin_ = MAKEPOINTS(longParameter);
        return 0;

    case WM_MOUSEMOVE:
        if (wideParameter == MK_LBUTTON)
        {
            const POINTS points = MAKEPOINTS(longParameter);
            RECT rect{};
            GetWindowRect(window_, &rect);
            rect.left += points.x - dragOrigin_.x;
            rect.top += points.y - dragOrigin_.y;

            if (dragOrigin_.x >= 0 && dragOrigin_.x <= gui::WIDTH &&
                dragOrigin_.y >= 0 && dragOrigin_.y <= 19)
            {
                SetWindowPos(
                    window_,
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

    return DefWindowProcW(window_, message, wideParameter, longParameter);
}
