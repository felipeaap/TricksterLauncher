#pragma once

#include <Windows.h>
#include <functional>
#include <utility>

class LauncherWindow
{
public:
    using ResizeCallback = std::function<void(UINT width, UINT height)>;
    using MoveCallback = std::function<void()>;

    LauncherWindow() = default;
    ~LauncherWindow();

    LauncherWindow(const LauncherWindow&) = delete;
    LauncherWindow& operator=(const LauncherWindow&) = delete;

    bool Create(const wchar_t* title) noexcept;
    void Destroy() noexcept;
    void Minimize() noexcept;
    bool PumpMessages() noexcept;

    static constexpr int kDefaultWidth = 538;
    static constexpr int kDefaultHeight = 526;

    HWND Handle() const noexcept { return window_; }
    bool IsRunning() const noexcept { return running_; }
    void SetRunning(bool running) noexcept { running_ = running; }

    void SetResizeCallback(ResizeCallback callback) { resizeCallback_ = std::move(callback); }
    void SetMoveCallback(MoveCallback callback) { moveCallback_ = std::move(callback); }

private:
    static LRESULT CALLBACK WindowProcess(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter) noexcept;

    HWND window_ = nullptr;
    HINSTANCE instance_ = nullptr;
    POINTS dragOrigin_{};
    bool classRegistered_ = false;
    bool running_ = true;
    ResizeCallback resizeCallback_;
    MoveCallback moveCallback_;

    static constexpr wchar_t kWindowClassName[] = L"class001";
};
