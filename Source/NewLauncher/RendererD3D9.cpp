#include "RendererD3D9.h"

#include "imgui_impl_dx9.h"

RendererD3D9::~RendererD3D9()
{
    Destroy();
}

void RendererD3D9::SetBackBufferSize(UINT width, UINT height) noexcept
{
    presentParameters_.BackBufferWidth = width;
    presentParameters_.BackBufferHeight = height;
}

bool RendererD3D9::Create(HWND window) noexcept
{
    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_)
        return false;

    ZeroMemory(&presentParameters_, sizeof(presentParameters_));
    presentParameters_.Windowed = TRUE;
    presentParameters_.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters_.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParameters_.EnableAutoDepthStencil = TRUE;
    presentParameters_.AutoDepthStencilFormat = D3DFMT_D16;
    presentParameters_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (FAILED(d3d_->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &presentParameters_,
            &device_)))
    {
        d3d_->Release();
        d3d_ = nullptr;
        return false;
    }

    return true;
}

void RendererD3D9::Reset() noexcept
{
    if (!device_)
        return;

    sceneBegun_ = false;
    ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT result = device_->Reset(&presentParameters_);
    if (result == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    if (SUCCEEDED(result))
        ImGui_ImplDX9_CreateDeviceObjects();
}

void RendererD3D9::Destroy() noexcept
{
    sceneBegun_ = false;

    if (device_)
    {
        device_->Release();
        device_ = nullptr;
    }

    if (d3d_)
    {
        d3d_->Release();
        d3d_ = nullptr;
    }
}

bool RendererD3D9::BeginFrame() noexcept
{
    sceneBegun_ = false;
    if (!device_)
        return false;

    device_->SetRenderState(D3DRS_ZENABLE, FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device_->Clear(
        0,
        nullptr,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_RGBA(0, 0, 0, 255),
        1.0f,
        0);

    sceneBegun_ = SUCCEEDED(device_->BeginScene());
    return sceneBegun_;
}

void RendererD3D9::EndFrame() noexcept
{
    if (!device_)
        return;

    if (sceneBegun_)
    {
        device_->EndScene();
        sceneBegun_ = false;
    }

    const HRESULT result = device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST &&
        device_->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
    {
        Reset();
    }
}
