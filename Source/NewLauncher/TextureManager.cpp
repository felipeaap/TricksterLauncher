#include "TextureManager.h"

#include <d3dx9.h>
#include "resource.h"

namespace
{
constexpr int kBgWidth = 538;
constexpr int kBgHeight = 564;
constexpr int kLogoWidth = 185;
constexpr int kLogoHeight = 71;
constexpr int kButtonOptionWidth = 113;
constexpr int kButtonOptionHeight = 27;
constexpr int kButtonExitWidth = 113;
constexpr int kButtonExitHeight = 27;
constexpr int kButtonGameWidth = 118;
constexpr int kButtonGameHeight = 61;
constexpr int kButtonCheckWidth = 113;
constexpr int kButtonCheckHeight = 27;
}

TextureManager::~TextureManager()
{
    ReleaseAll();
}

void TextureManager::SafeRelease(IDirect3DTexture9*& tex) noexcept
{
    if (tex)
    {
        tex->Release();
        tex = nullptr;
    }
}

void TextureManager::ReleaseAll() noexcept
{
    SafeRelease(bgTex_);
    SafeRelease(logoTex_);

    SafeRelease(optionPair_.color);
    SafeRelease(optionPair_.gray);
    SafeRelease(optionHover_);
    SafeRelease(optionSelected_);

    SafeRelease(exitNormal_);
    SafeRelease(exitHover_);
    SafeRelease(exitSelected_);

    SafeRelease(gamePair_.color);
    SafeRelease(gamePair_.gray);
    SafeRelease(gameHover_);
    SafeRelease(gameSelected_);

    SafeRelease(checkPair_.color);
    SafeRelease(checkPair_.gray);
    SafeRelease(checkHover_);
    SafeRelease(checkSelected_);
}

void TextureManager::OnDeviceLost() noexcept
{
    ReleaseAll();
}

bool TextureManager::OnDeviceReset(IDirect3DDevice9* device, HINSTANCE instance) noexcept
{
    return LoadAll(device, instance);
}

IDirect3DTexture9* TextureManager::LoadTextureFromResource(
    IDirect3DDevice9* device,
    HINSTANCE instance,
    int resourceId,
    int desiredWidth,
    int desiredHeight) noexcept
{
    if (!device)
        return nullptr;

    HRSRC hRes = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), L"PNG");
    if (!hRes)
        return nullptr;

    DWORD resSize = SizeofResource(instance, hRes);
    HGLOBAL hMem = LoadResource(instance, hRes);
    void* pData = LockResource(hMem);
    if (!pData)
        return nullptr;

    IDirect3DTexture9* texture = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
        device,
        pData,
        resSize,
        desiredWidth,
        desiredHeight,
        D3DX_DEFAULT,
        0,
        D3DFMT_UNKNOWN,
        D3DPOOL_DEFAULT,
        D3DX_FILTER_NONE,
        D3DX_FILTER_NONE,
        0,
        nullptr,
        nullptr,
        &texture);

    return SUCCEEDED(hr) ? texture : nullptr;
}

TexturePair TextureManager::LoadTexturePairFromResource(
    IDirect3DDevice9* device,
    HINSTANCE instance,
    int resourceId,
    int desiredWidth,
    int desiredHeight) noexcept
{
    TexturePair result = { nullptr, nullptr };
    if (!device)
        return result;

    HRSRC hRes = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), L"PNG");
    if (!hRes)
        return result;

    DWORD resSize = SizeofResource(instance, hRes);
    HGLOBAL hMem = LoadResource(instance, hRes);
    void* pData = LockResource(hMem);
    if (!pData)
        return result;

    IDirect3DTexture9* texColor = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
        device,
        pData,
        resSize,
        desiredWidth,
        desiredHeight,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        D3DX_FILTER_NONE,
        D3DX_FILTER_NONE,
        0,
        nullptr,
        nullptr,
        &texColor);

    if (FAILED(hr) || !texColor)
        return result;

    result.color = texColor;

    IDirect3DTexture9* texGray = nullptr;
    hr = device->CreateTexture(
        desiredWidth,
        desiredHeight,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &texGray,
        nullptr);

    if (FAILED(hr) || !texGray)
        return result;

    D3DLOCKED_RECT rectSrc{};
    D3DLOCKED_RECT rectDst{};
    texColor->LockRect(0, &rectSrc, nullptr, D3DLOCK_READONLY);
    texGray->LockRect(0, &rectDst, nullptr, 0);

    for (int y = 0; y < desiredHeight; y++)
    {
        auto* src = reinterpret_cast<DWORD*>(static_cast<BYTE*>(rectSrc.pBits) + y * rectSrc.Pitch);
        auto* dst = reinterpret_cast<DWORD*>(static_cast<BYTE*>(rectDst.pBits) + y * rectDst.Pitch);
        for (int x = 0; x < desiredWidth; x++)
        {
            DWORD c = src[x];
            BYTE a = (c >> 24) & 0xFF;
            BYTE r = (c >> 16) & 0xFF;
            BYTE g = (c >> 8) & 0xFF;
            BYTE b = (c >> 0) & 0xFF;
            BYTE gray = static_cast<BYTE>(0.299f * r + 0.587f * g + 0.114f * b);
            dst[x] = (a << 24) | (gray << 16) | (gray << 8) | gray;
        }
    }

    texColor->UnlockRect(0);
    texGray->UnlockRect(0);
    result.gray = texGray;
    return result;
}

bool TextureManager::LoadAll(IDirect3DDevice9* device, HINSTANCE instance) noexcept
{
    if (!device)
        return false;

    if (!instance)
        instance = GetModuleHandleW(nullptr);

    ReleaseAll();

    bgTex_ = LoadTextureFromResource(device, instance, IDB_BG, kBgWidth, kBgHeight);
    logoTex_ = LoadTextureFromResource(device, instance, IDB_LOGO, kLogoWidth, kLogoHeight);

    optionPair_ = LoadTexturePairFromResource(device, instance, OPTION_N, kButtonOptionWidth, kButtonOptionHeight);
    optionHover_ = LoadTextureFromResource(device, instance, OPTION_H, kButtonOptionWidth, kButtonOptionHeight);
    optionSelected_ = LoadTextureFromResource(device, instance, OPTION_S, kButtonOptionWidth, kButtonOptionHeight);

    exitNormal_ = LoadTextureFromResource(device, instance, EXIT_N, kButtonExitWidth, kButtonExitHeight);
    exitHover_ = LoadTextureFromResource(device, instance, EXIT_H, kButtonExitWidth, kButtonExitHeight);
    exitSelected_ = LoadTextureFromResource(device, instance, EXIT_S, kButtonExitWidth, kButtonExitHeight);

    gamePair_ = LoadTexturePairFromResource(device, instance, GAME_N, kButtonGameWidth, kButtonGameHeight);
    gameHover_ = LoadTextureFromResource(device, instance, GAME_H, kButtonGameWidth, kButtonGameHeight);
    gameSelected_ = LoadTextureFromResource(device, instance, GAME_S, kButtonGameWidth, kButtonGameHeight);

    checkPair_ = LoadTexturePairFromResource(device, instance, CHECK_N, kButtonCheckWidth, kButtonCheckHeight);
    checkHover_ = LoadTextureFromResource(device, instance, CHECK_H, kButtonCheckWidth, kButtonCheckHeight);
    checkSelected_ = LoadTextureFromResource(device, instance, CHECK_S, kButtonCheckWidth, kButtonCheckHeight);

    return (bgTex_ != nullptr);
}
