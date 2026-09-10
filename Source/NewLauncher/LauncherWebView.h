#pragma once

#include <Windows.h>
#include <string>
#include <wil/com.h>
#include <WebView2.h>

class LauncherWebView
{
public:
    LauncherWebView() = default;
    ~LauncherWebView();

    LauncherWebView(const LauncherWebView&) = delete;
    LauncherWebView& operator=(const LauncherWebView&) = delete;

    bool Initialize(HWND parent, const std::wstring& url) noexcept;
    void SetBounds(const RECT& bounds) noexcept;
    void Navigate(const std::wstring& url) noexcept;
    void Shutdown() noexcept;

    bool IsReady() const noexcept { return webview_ != nullptr; }

private:
    wil::com_ptr<ICoreWebView2Controller> controller_;
    wil::com_ptr<ICoreWebView2> webview_;
    RECT bounds_{19, 32, 19 + 503, 32 + 343};
    std::wstring pendingUrl_;
};
