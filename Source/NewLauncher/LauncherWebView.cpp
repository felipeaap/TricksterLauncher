#include "LauncherWebView.h"

#include <filesystem>
#include <wrl.h>

#include "Config.h"

namespace
{
constexpr RECT kDefaultBounds{19, 32, 19 + 503, 32 + 343};
}

LauncherWebView::~LauncherWebView()
{
    Shutdown();
}

bool LauncherWebView::Initialize(HWND parent, const std::wstring& url) noexcept
{
    if (!parent)
        return false;

    pendingUrl_ = url;
    bounds_ = kDefaultBounds;

    wchar_t tempPath[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
        return false;

    const std::filesystem::path cacheFolder =
        std::filesystem::path(tempPath) / L"WebView2Cache";

    std::error_code error;
    std::filesystem::create_directories(cacheFolder, error);

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        cacheFolder.c_str(),
        nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, parent](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT
            {
                if (FAILED(result) || !environment)
                    return FAILED(result) ? result : E_FAIL;

                return environment->CreateCoreWebView2Controller(
                    parent,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(controllerResult) || !controller)
                                return FAILED(controllerResult) ? controllerResult : E_FAIL;

                            controller_ = controller;
                            controller_->get_CoreWebView2(&webview_);
                            if (!webview_)
                                return E_FAIL;

                            SetBounds(bounds_);
                            if (!pendingUrl_.empty())
                                webview_->Navigate(pendingUrl_.c_str());
                            return S_OK;
                        }
                    ).Get());
            }
        ).Get());

    return SUCCEEDED(hr);
}

void LauncherWebView::SetBounds(const RECT& bounds) noexcept
{
    bounds_ = bounds;
    if (controller_)
        controller_->put_Bounds(bounds_);
}

void LauncherWebView::Navigate(const std::wstring& url) noexcept
{
    pendingUrl_ = url;
    if (webview_ && !pendingUrl_.empty())
        webview_->Navigate(pendingUrl_.c_str());
}

void LauncherWebView::Shutdown() noexcept
{
    webview_.reset();
    if (controller_)
    {
        controller_->Close();
        controller_.reset();
    }
    pendingUrl_.clear();
}
