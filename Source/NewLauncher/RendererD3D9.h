#pragma once

#include <Windows.h>
#include <d3d9.h>

class RendererD3D9
{
public:
    RendererD3D9(PDIRECT3D9& d3d,
                LPDIRECT3DDEVICE9& device,
                D3DPRESENT_PARAMETERS& presentParameters);

    bool Create(HWND window) noexcept;
    void Reset() noexcept;
    void Destroy() noexcept;
    bool BeginFrame() noexcept;
    void EndFrame() noexcept;

private:
    PDIRECT3D9& d3d_;
    LPDIRECT3DDEVICE9& device_;
    D3DPRESENT_PARAMETERS& presentParameters_;
};
