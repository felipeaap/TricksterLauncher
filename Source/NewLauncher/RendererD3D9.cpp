#include "RendererD3D9.h"

#include "Logger.h"
#include "imgui_impl_dx9.h"

RendererD3D9::~RendererD3D9()
{
    Destroy();
}

void RendererD3D9::SetBackBufferSize(UINT width, UINT height) noexcept
{
    presentParameters_.BackBufferWidth  = width;
    presentParameters_.BackBufferHeight = height;
}

void RendererD3D9::SetDeviceResetCallbacks(
    std::function<void()> onBeforeReset,
    std::function<void()> onAfterReset) noexcept
{
    onBeforeReset_ = std::move(onBeforeReset);
    onAfterReset_  = std::move(onAfterReset);
}

bool RendererD3D9::Create(HWND window) noexcept
{
    Logger::Log("RendererD3D9::Create");

    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_)
    {
        Logger::LogError("RendererD3D9::Create: Direct3DCreate9 returned null");
        return false;
    }

    D3DDISPLAYMODE d3ddm{};
    d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

    D3DPRESENT_PARAMETERS ppConfigs[] = {
        // Config 0: Minimal standard windowed
        { 0, 0, D3DFMT_UNKNOWN, 0, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD,
          window, TRUE, FALSE, D3DFMT_UNKNOWN, 0, D3DPRESENT_INTERVAL_DEFAULT },
        // Config 1: Explicit dimensions
        { 538, 564, D3DFMT_UNKNOWN, 0, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD,
          window, TRUE, FALSE, D3DFMT_UNKNOWN, 0, D3DPRESENT_INTERVAL_DEFAULT },
        // Config 2: Adapter display format
        { 0, 0, d3ddm.Format, 0, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD,
          window, TRUE, FALSE, D3DFMT_UNKNOWN, 0, D3DPRESENT_INTERVAL_DEFAULT },
        // Config 3: SwapEffect COPY
        { 0, 0, D3DFMT_UNKNOWN, 0, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_COPY,
          window, TRUE, FALSE, D3DFMT_UNKNOWN, 0, D3DPRESENT_INTERVAL_DEFAULT }
    };

    HRESULT hr = E_FAIL;

    // Try Direct3DCreate9Ex first (modern Windows / WARP / VM compatibility)
    typedef HRESULT(WINAPI* LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);
    HMODULE hD3D9 = GetModuleHandleW(L"d3d9.dll");
    if (!hD3D9) hD3D9 = LoadLibraryW(L"d3d9.dll");
    if (hD3D9)
    {
        auto pCreate9Ex = reinterpret_cast<LPDIRECT3DCREATE9EX>(
            GetProcAddress(hD3D9, "Direct3DCreate9Ex"));
        if (pCreate9Ex)
        {
            IDirect3D9Ex* d3dEx = nullptr;
            if (SUCCEEDED(pCreate9Ex(D3D_SDK_VERSION, &d3dEx)) && d3dEx)
            {
                IDirect3DDevice9Ex* deviceEx = nullptr;
                for (size_t c = 0; c < sizeof(ppConfigs) / sizeof(ppConfigs[0]); ++c)
                {
                    presentParameters_ = ppConfigs[c];
                    hr = d3dEx->CreateDeviceEx(
                        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                        D3DCREATE_HARDWARE_VERTEXPROCESSING,
                        &presentParameters_, nullptr, &deviceEx);
                    if (FAILED(hr))
                    {
                        hr = d3dEx->CreateDeviceEx(
                            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                            &presentParameters_, nullptr, &deviceEx);
                    }
                    if (SUCCEEDED(hr) && deviceEx)
                    {
                        d3d_->Release();
                        d3d_    = d3dEx;
                        device_ = deviceEx;
                        Logger::Log("RendererD3D9::Create: D3D9Ex device created (cfg=" + std::to_string(c) + ")");
                        return true;
                    }
                }
                d3dEx->Release();
            }
        }
    }

    // Fallback: classic Direct3D 9
    const DWORD flags[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING
    };
    const D3DDEVTYPE devTypes[] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };

    hr = E_FAIL;
    for (size_t c = 0; c < sizeof(ppConfigs) / sizeof(ppConfigs[0]); ++c)
    {
        for (const auto devType : devTypes)
        {
            for (const auto flag : flags)
            {
                presentParameters_ = ppConfigs[c];
                hr = d3d_->CreateDevice(
                    D3DADAPTER_DEFAULT, devType, window,
                    flag, &presentParameters_, &device_);
                if (SUCCEEDED(hr))
                    break;
            }
            if (SUCCEEDED(hr)) break;
        }
        if (SUCCEEDED(hr)) break;
    }

    if (FAILED(hr))
    {
        d3d_->Release();
        d3d_ = nullptr;
        Logger::LogError("RendererD3D9::Create: all CreateDevice attempts failed");
        return false;
    }

    Logger::Log("RendererD3D9::Create: D3D9 device created successfully");
    return true;
}

void RendererD3D9::Reset() noexcept
{
    if (!device_)
        return;

    // Notify consumers to release D3DPOOL_DEFAULT resources
    if (onBeforeReset_)
        onBeforeReset_();

    sceneBegun_ = false;
    ImGui_ImplDX9_InvalidateDeviceObjects();

    const HRESULT result = device_->Reset(&presentParameters_);
    if (result == D3DERR_INVALIDCALL)
    {
        Logger::LogError("RendererD3D9::Reset: D3DERR_INVALIDCALL");
        IM_ASSERT(0);
        return;
    }

    if (SUCCEEDED(result))
    {
        ImGui_ImplDX9_CreateDeviceObjects();

        // Notify consumers to recreate D3DPOOL_DEFAULT resources
        if (onAfterReset_)
            onAfterReset_();

        Logger::Log("RendererD3D9::Reset: device reset successful");
    }
    else
    {
        Logger::LogError("RendererD3D9::Reset: failed hr=" + std::to_string(result));
    }
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

    device_->SetRenderState(D3DRS_ZENABLE,            FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE,   FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE,  FALSE);
    device_->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

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
        Logger::Log("RendererD3D9::EndFrame: device lost — resetting");
        Reset();
    }
}
