#pragma once

#include <Windows.h>
#include <d3d9.h>

class RendererD3D9
{
public:
    RendererD3D9() = default;
    ~RendererD3D9();

    // Non-copyable
    RendererD3D9(const RendererD3D9&) = delete;
    RendererD3D9& operator=(const RendererD3D9&) = delete;

    bool Create(HWND window) noexcept;
    void Reset() noexcept;
    void Destroy() noexcept;
    bool BeginFrame() noexcept;
    void EndFrame() noexcept;

    void SetBackBufferSize(UINT width, UINT height) noexcept;

    [[nodiscard]] LPDIRECT3DDEVICE9 Device() const noexcept { return device_; }
    [[nodiscard]] PDIRECT3D9 D3D() const noexcept { return d3d_; }
    [[nodiscard]] const D3DPRESENT_PARAMETERS& PresentParameters() const noexcept { return presentParameters_; }
    [[nodiscard]] D3DPRESENT_PARAMETERS& PresentParameters() noexcept { return presentParameters_; }

private:
    PDIRECT3D9 d3d_ = nullptr;
    LPDIRECT3DDEVICE9 device_ = nullptr;
    D3DPRESENT_PARAMETERS presentParameters_{};
    bool sceneBegun_ = false;
};

