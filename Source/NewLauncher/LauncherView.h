#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include "GameSettings.h"
#include "LauncherState.h"
#include "imgui.h"

enum class ButtonIcon
{
    None,
    Play,
    Check,
    Settings,
    Exit
};

struct LauncherViewEvents
{
    std::function<void()> onPlayClicked;
    std::function<void()> onCheckClicked;
    std::function<void()> onOptionClicked;
    std::function<void()> onExitClicked;
    std::function<void()> onMinimizeClicked;
    std::function<void(const std::string& url)> onLinkClicked;
    std::function<void(const std::string& account, const std::string& password, bool saveAccount)> onConnectClicked;
};

class LauncherView
{
public:
    LauncherView() = default;
    ~LauncherView() = default;

    static constexpr int kWidth = 538;
    static constexpr int kHeight = 480;

    void Initialize() noexcept;
    void Render(
        const LauncherViewState& state,
        const LauncherViewEvents& events) noexcept;

    bool ShouldClose() const noexcept { return shouldClose_; }
    void RequestClose() noexcept { shouldClose_ = true; }

    void SetSavedAccount(const std::string& account, bool remember = true) noexcept;
    void SetAuthError(const std::string& error) noexcept { authErrorText_ = error; }

    void OpenSettings() noexcept;
    void CloseSettings() noexcept { showSettingsModal_ = false; }
    bool IsSettingsOpen() const noexcept { return showSettingsModal_; }

    void LoadHeroTexture(struct IDirect3DDevice9* device, const std::wstring& exeDir = L"") noexcept;
    void UnloadHeroTexture() noexcept;
    void LoadDrillTexture(struct IDirect3DDevice9* device, const std::wstring& exeDir = L"") noexcept;
    void UnloadDrillTexture() noexcept;

private:
    bool ModernButton(
        const char* id,
        const char* label,
        const ImVec2& size,
        bool isLocked,
        ButtonIcon icon = ButtonIcon::None,
        ImU32 accentColor = IM_COL32(0, 162, 237, 255),
        bool pulseGlow = false) noexcept;

    static void RenderButtonIcon(
        ButtonIcon icon,
        const ImVec2& center,
        float size,
        ImU32 color,
        ImDrawList* dl) noexcept;

    static void RenderLink(
        const char* label,
        const char* url,
        const std::function<void(const std::string&)>& onClick) noexcept;

    void RenderBeveledProgressBar(
        float fraction,
        const ImVec2& size,
        ImU32 colTop,
        ImU32 colBottom,
        ImU32 colHighlight,
        ImU32 colShadow,
        ImU32 colBorder,
        float rounding = 4.0f,
        bool showPercentage = false) noexcept;

    void RenderTricksterProgressBar(
        float fraction,
        const ImVec2& size,
        bool showGears = true,
        bool showDrill = true,
        bool showRulerNumbers = true,
        ImFont* fontSmall = nullptr) noexcept;

    void RenderLoginForm(
        float bottomCardY,
        const ImVec2& winSize,
        const LauncherViewState& state,
        const LauncherViewEvents& events) noexcept;

    void RenderSettingsModal(const ImVec2& winSize) noexcept;

    bool shouldClose_ = false;
    bool showLoginForm_ = false;
    float loginFormAnim_ = 0.0f;
    char accountBuffer_[64] = { 0 };
    char passwordBuffer_[64] = { 0 };
    bool rememberAccount_ = true;
    std::string authErrorText_;

    bool showSettingsModal_ = false;
    float settingsModalAnim_ = 0.0f;
    int settingsActiveTab_ = 0;
    GameConfig pendingSettings_;
    float settingsToastTimer_ = 0.0f;

    ImFont* fontSmall_ = nullptr;
    ImFont* fontRegular_ = nullptr;
    ImFont* fontBold_ = nullptr;
    ImFont* fontLarge_ = nullptr;
    float animFileProgress_ = 0.0f;
    float animTotalProgress_ = 0.0f;
    float gameStartUnlockSweep_ = 0.0f;
    bool wasGameEnabled_ = false;
    std::unordered_map<std::string, float> buttonHover_;
    struct IDirect3DTexture9* heroTexture_ = nullptr;
    ImVec2 heroTexUvMin_{ 0.0f, 0.0f };
    ImVec2 heroTexUvMax_{ 1.0f, 1.0f };
    struct IDirect3DTexture9* drillTexture_ = nullptr;
    unsigned int drillTexW_ = 0;
    unsigned int drillTexH_ = 0;
    float drillCellW_ = 49.0f;
    float drillCellH_ = 86.0f;
    int drillFrameCount_ = 6;
    std::wstring selectedDrillDir_;
    std::wstring heroExeDir_;
};
