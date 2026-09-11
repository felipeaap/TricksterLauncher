#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>

class Helper;

namespace gui
{
    constexpr int WIDTH = 538;
    constexpr int HEIGHT = 564;

    void SetWindow(HWND window) noexcept;
    HWND GetWindow() noexcept;

    bool CreateDevice() noexcept;
    void ResetDevice() noexcept;
    void DestroyDevice() noexcept;
    LPDIRECT3DDEVICE9 GetDevice() noexcept;

    void HandleResize(UINT width, UINT height) noexcept;

    void CreateImGui() noexcept;
    void DestroyImGui() noexcept;
    void LoadResources(HINSTANCE hInstance = nullptr) noexcept;

    void BeginRender() noexcept;
    void Render() noexcept;
    void EndRender() noexcept;

    bool IsRunning() noexcept;
    void SetRunning(bool running) noexcept;

    void OpenURL(const char* url) noexcept;
    void RenderLink(const char* label, const char* url) noexcept;
}

