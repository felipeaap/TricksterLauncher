#pragma once

#include <functional>
#include <string>
#include "LauncherState.h"
#include "TextureManager.h"
#include "imgui.h"

struct LauncherViewEvents
{
    std::function<void()> onPlayClicked;
    std::function<void()> onCheckClicked;
    std::function<void()> onOptionClicked;
    std::function<void()> onExitClicked;
    std::function<void()> onMinimizeClicked;
    std::function<void(const std::string& url)> onLinkClicked;
};

class LauncherView
{
public:
    LauncherView() = default;
    ~LauncherView() = default;

    static constexpr int kWidth = 538;
    static constexpr int kHeight = 564;

    void Initialize() noexcept;
    void Render(
        const LauncherViewState& state,
        const TextureManager& textures,
        const LauncherViewEvents& events) noexcept;

    bool ShouldClose() const noexcept { return shouldClose_; }
    void RequestClose() noexcept { shouldClose_ = true; }

private:
    static bool ImageButton(
        const char* id,
        IDirect3DTexture9* normal,
        IDirect3DTexture9* hover,
        IDirect3DTexture9* pressed,
        IDirect3DTexture9* locked,
        const ImVec2& size,
        bool isLocked,
        const char* fallbackLabel = nullptr,
        ImU32 fallbackAccent = IM_COL32(0, 162, 237, 255)) noexcept;

    static bool ModernButton(
        const char* id,
        const char* label,
        const ImVec2& size,
        bool isLocked,
        ImU32 accentColor = IM_COL32(0, 162, 237, 255),
        bool pulseGlow = false) noexcept;

    static void RenderLink(
        const char* label,
        const char* url,
        const std::function<void(const std::string&)>& onClick) noexcept;

    static void RenderBeveledProgressBar(
        float fraction,
        const ImVec2& size,
        ImU32 colTop,
        ImU32 colBottom,
        ImU32 colHighlight,
        ImU32 colShadow,
        ImU32 colBorder,
        float rounding = 4.0f,
        bool showPercentage = false) noexcept;

    bool shouldClose_ = false;
    ImFont* fontSmall_ = nullptr;
    ImFont* fontRegular_ = nullptr;
    ImFont* fontBold_ = nullptr;
    ImFont* fontLarge_ = nullptr;
    float animFileProgress_ = 0.0f;
    float animTotalProgress_ = 0.0f;
};
