#pragma once

#include <d3d9.h>
#define NOMINMAX
#include <windows.h>

struct TexturePair
{
    IDirect3DTexture9* color = nullptr;
    IDirect3DTexture9* gray = nullptr;
};

class TextureManager
{
public:
    TextureManager() = default;
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    bool LoadAll(IDirect3DDevice9* device, HINSTANCE instance = nullptr) noexcept;
    void ReleaseAll() noexcept;

    /// Call before IDirect3DDevice9::Reset() — releases all D3DPOOL_DEFAULT textures.
    void OnDeviceLost() noexcept;

    /// Call after IDirect3DDevice9::Reset() succeeded — reloads all textures.
    /// @return true if background texture was reloaded successfully.
    bool OnDeviceReset(IDirect3DDevice9* device, HINSTANCE instance = nullptr) noexcept;

    IDirect3DTexture9* Background() const noexcept { return bgTex_; }
    IDirect3DTexture9* Logo() const noexcept { return logoTex_; }

    IDirect3DTexture9* OptionNormal() const noexcept { return optionPair_.color; }
    IDirect3DTexture9* OptionHover() const noexcept { return optionHover_; }
    IDirect3DTexture9* OptionSelected() const noexcept { return optionSelected_; }
    IDirect3DTexture9* OptionGray() const noexcept { return optionPair_.gray; }

    IDirect3DTexture9* ExitNormal() const noexcept { return exitNormal_; }
    IDirect3DTexture9* ExitHover() const noexcept { return exitHover_; }
    IDirect3DTexture9* ExitSelected() const noexcept { return exitSelected_; }

    IDirect3DTexture9* GameNormal() const noexcept { return gamePair_.color; }
    IDirect3DTexture9* GameHover() const noexcept { return gameHover_; }
    IDirect3DTexture9* GameSelected() const noexcept { return gameSelected_; }
    IDirect3DTexture9* GameGray() const noexcept { return gamePair_.gray; }

    IDirect3DTexture9* CheckNormal() const noexcept { return checkPair_.color; }
    IDirect3DTexture9* CheckHover() const noexcept { return checkHover_; }
    IDirect3DTexture9* CheckSelected() const noexcept { return checkSelected_; }
    IDirect3DTexture9* CheckGray() const noexcept { return checkPair_.gray; }

private:
    static IDirect3DTexture9* LoadTextureFromResource(
        IDirect3DDevice9* device,
        HINSTANCE instance,
        int resourceId,
        int desiredWidth,
        int desiredHeight) noexcept;

    static TexturePair LoadTexturePairFromResource(
        IDirect3DDevice9* device,
        HINSTANCE instance,
        int resourceId,
        int desiredWidth,
        int desiredHeight) noexcept;

    static void SafeRelease(IDirect3DTexture9*& tex) noexcept;

    IDirect3DTexture9* bgTex_ = nullptr;
    IDirect3DTexture9* logoTex_ = nullptr;

    TexturePair optionPair_{};
    IDirect3DTexture9* optionHover_ = nullptr;
    IDirect3DTexture9* optionSelected_ = nullptr;

    IDirect3DTexture9* exitNormal_ = nullptr;
    IDirect3DTexture9* exitHover_ = nullptr;
    IDirect3DTexture9* exitSelected_ = nullptr;

    TexturePair gamePair_{};
    IDirect3DTexture9* gameHover_ = nullptr;
    IDirect3DTexture9* gameSelected_ = nullptr;

    TexturePair checkPair_{};
    IDirect3DTexture9* checkHover_ = nullptr;
    IDirect3DTexture9* checkSelected_ = nullptr;
};
