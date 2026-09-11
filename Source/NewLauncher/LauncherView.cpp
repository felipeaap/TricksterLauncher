#include "LauncherView.h"

#include <algorithm>
#define NOMINMAX
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "Config.h"
#include "Language.h"

void LauncherView::Initialize() noexcept
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    char windowsDir[MAX_PATH]{};
    GetWindowsDirectoryA(windowsDir, MAX_PATH);
    std::string fontPath = std::string(windowsDir) + "\\Fonts\\segoeuib.ttf";

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    const float baseFontSize = 16.0f;
    const float smallFontSize = baseFontSize * 0.8f;

    auto LoadFont = [&](const char* path, float size) -> ImFont*
    {
        ImFontConfig fontCfg;
        fontCfg.MergeMode = false;
        return io.Fonts->AddFontFromFileTTF(path, size, &fontCfg, glyphRanges);
    };

    fontSmall_ = LoadFont(fontPath.c_str(), smallFontSize);
    fontRegular_ = LoadFont(fontPath.c_str(), baseFontSize);
    io.FontDefault = fontRegular_ ? fontRegular_ : io.Fonts->Fonts[0];
    io.Fonts->Build();
    io.IniFilename = nullptr;
}

bool LauncherView::ImageButton(
    const char* id,
    IDirect3DTexture9* normal,
    IDirect3DTexture9* hover,
    IDirect3DTexture9* pressed,
    IDirect3DTexture9* locked,
    const ImVec2& size,
    bool isLocked) noexcept
{
    ImGui::PushID(id);
    bool pressedResult = ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImTextureID tex = reinterpret_cast<ImTextureID>(normal);
    if (!isLocked)
    {
        if (held && pressed)
            tex = reinterpret_cast<ImTextureID>(pressed);
        else if (hovered && hover)
            tex = reinterpret_cast<ImTextureID>(hover);
    }
    else if (locked)
    {
        tex = reinterpret_cast<ImTextureID>(locked);
    }

    if (tex)
        dl->AddImage(tex, min, max);

    ImGui::PopID();
    return pressedResult;
}

void LauncherView::RenderLink(
    const char* label,
    const char* url,
    const std::function<void(const std::string&)>& onClick) noexcept
{
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 209, 97, 255));
    if (ImGui::Text("%s", label); ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked())
        {
            if (onClick)
                onClick(url ? url : "");
        }
    }
    ImGui::PopStyleColor();
}

void LauncherView::RenderBeveledProgressBar(
    float fraction,
    const ImVec2& size,
    ImU32 colTop,
    ImU32 colBottom,
    ImU32 colHighlight,
    ImU32 colShadow,
    ImU32 colBorder,
    float rounding) noexcept
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Inset background track (soft outer shadow + sunken slot)
    dl->AddRectFilled(ImVec2(p0.x - 1.0f, p0.y - 1.0f), ImVec2(p1.x + 1.0f, p1.y + 1.0f), IM_COL32(0, 0, 0, 60), rounding + 1.0f);
    dl->AddRectFilled(p0, p1, IM_COL32(10, 15, 24, 230), rounding);

    // Soft upper inner shadow for depth
    const float halfTrackH = size.y * 0.45f;
    dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + halfTrackH), IM_COL32(0, 0, 0, 60), rounding, ImDrawFlags_RoundCornersTop);

    // Subtle track border
    dl->AddRect(p0, p1, IM_COL32(35, 50, 72, 175), rounding, 0, 1.0f);

    // Subtle bottom 3D groove highlight
    dl->AddLine(ImVec2(p0.x + rounding, p1.y + 0.5f), ImVec2(p1.x - rounding, p1.y + 0.5f), IM_COL32(255, 255, 255, 24), 1.0f);

    // 2. Filled progress bar
    const float f = (std::clamp)(fraction, 0.0f, 1.0f);
    const float pad = 1.0f;
    const ImVec2 barP0 = ImVec2(p0.x + pad, p0.y + pad);
    const float maxBarW = size.x - pad * 2.0f;
    const float barH = size.y - pad * 2.0f;
    const float barW = maxBarW * f;

    if (barW >= 2.0f)
    {
        const ImVec2 barP1 = ImVec2(barP0.x + barW, barP0.y + barH);
        const float barRounding = (std::min)(rounding - 0.5f, barW * 0.5f);
        const ImDrawFlags cornerFlags = (barW >= maxBarW - 1.0f) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft;

        // Base fill (smooth rich tone)
        dl->AddRectFilled(barP0, barP1, colBottom, barRounding, cornerFlags);

        // Soft layered bevel gloss dome (smooth multi-layer gradient without harsh lines)
        const ImDrawFlags topCornerFlags = (cornerFlags & ImDrawFlags_RoundCornersLeft)
            ? (ImDrawFlags_RoundCornersTopLeft | (barW >= maxBarW - 1.0f ? ImDrawFlags_RoundCornersTopRight : 0))
            : ImDrawFlags_None;

        // Layer 1: Broad soft upper sheen
        dl->AddRectFilled(barP0, ImVec2(barP1.x, barP0.y + barH * 0.52f), colTop, barRounding, topCornerFlags);

        // Layer 2: Subtle top-quarter highlight for rounded dome curve
        ImU32 colTopCrest = (colTop & 0x00FFFFFF) | (static_cast<ImU32>((colTop >> 24) * 0.7f) << 24);
        dl->AddRectFilled(barP0, ImVec2(barP1.x, barP0.y + barH * 0.28f), colTopCrest, barRounding, topCornerFlags);

        // Animated soft wave shimmer gliding across the bar
        const float t = static_cast<float>(ImGui::GetTime());
        const float shimmerPhase = fmodf(t * 0.65f, 2.0f) - 0.5f;
        const float shimmerCenter = barP0.x + maxBarW * shimmerPhase;
        const float shimmerWidth = 32.0f;
        const float sMinX = (std::max)(barP0.x, shimmerCenter - shimmerWidth * 0.5f);
        const float sMaxX = (std::min)(barP1.x, shimmerCenter + shimmerWidth * 0.5f);
        if (sMaxX > sMinX)
        {
            dl->AddRectFilled(
                ImVec2(sMinX, barP0.y),
                ImVec2(sMaxX, barP1.y),
                IM_COL32(255, 255, 255, 28),
                barRounding,
                topCornerFlags);
        }

        // Soft pulse on the leading tip when progressing
        if (f > 0.02f && f < 0.999f)
        {
            const float pulseAlpha = (sinf(t * 5.0f) * 0.5f + 0.5f) * 35.0f + 15.0f;
            dl->AddRectFilled(
                ImVec2(barP1.x - 2.5f, barP0.y),
                barP1,
                IM_COL32(255, 255, 255, static_cast<int>(pulseAlpha)),
                barRounding,
                cornerFlags);
        }

        // Soft specular top highlight line
        const float startX = barP0.x + ((topCornerFlags & ImDrawFlags_RoundCornersTopLeft) ? barRounding : 1.0f);
        const float endX = barP1.x - ((topCornerFlags & ImDrawFlags_RoundCornersTopRight) ? barRounding : 1.0f);
        if (endX > startX)
        {
            dl->AddLine(ImVec2(startX, barP0.y + 0.5f), ImVec2(endX, barP0.y + 0.5f), colHighlight, 1.0f);
        }

        // Soft bottom bevel shadow line
        const float shadowStartX = barP0.x + ((cornerFlags & ImDrawFlags_RoundCornersBottomLeft) ? barRounding : 1.0f);
        const float shadowEndX = barP1.x - 1.0f;
        if (shadowEndX > shadowStartX)
        {
            dl->AddLine(ImVec2(shadowStartX, barP1.y - 0.5f), ImVec2(shadowEndX, barP1.y - 0.5f), colShadow, 1.0f);
        }

        // Soft fill border
        dl->AddRect(barP0, barP1, colBorder, barRounding, cornerFlags, 1.0f);
    }

    // Advance ImGui cursor
    ImGui::Dummy(size);
}

void LauncherView::Render(
    const LauncherViewState& state,
    const TextureManager& textures,
    const LauncherViewEvents& events) noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ static_cast<float>(kWidth), static_cast<float>(kHeight) });

    // Smooth animated interpolation towards target progress values
    float dt = io.DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    const float lerpSpeed = 14.0f;
    const float lerpFactor = 1.0f - expf(-lerpSpeed * dt);
    animFileProgress_ += (state.fileProgress - animFileProgress_) * lerpFactor;
    animTotalProgress_ += (state.totalProgress - animTotalProgress_) * lerpFactor;

    if (fabsf(animFileProgress_ - state.fileProgress) < 0.0005f)
        animFileProgress_ = state.fileProgress;
    if (fabsf(animTotalProgress_ - state.totalProgress) < 0.0005f)
        animTotalProgress_ = state.totalProgress;

    bool showWindow = true;
    bool opened = ImGui::Begin(
        "##MainWindow",
        &showWindow,
        nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (opened)
    {
        if (textures.Background())
        {
            ImGui::GetWindowDrawList()->AddImage(
                reinterpret_cast<ImTextureID>(textures.Background()),
                ImVec2(0, 0),
                io.DisplaySize);
        }

        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec2 subTitleSize = ImGui::CalcTextSize(config::SubTitle.c_str());
        ImGui::SetCursorPos(ImVec2(winSize.x - subTitleSize.x - 15, 8));
        ImGui::Text("%s", config::SubTitle.c_str());

        if (textures.Logo())
        {
            ImGui::SetCursorPos(ImVec2(11, winSize.y - 98));
            ImGui::Image(
                reinterpret_cast<ImTextureID>(textures.Logo()),
                ImVec2(185, 71));
        }

        // Progress bar 1: File progress (Soft Ice Silver Bevel with smooth animation)
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 166));
        RenderBeveledProgressBar(
            animFileProgress_,
            ImVec2(362, 8),
            IM_COL32(255, 255, 255, 130),
            IM_COL32(185, 205, 230, 255),
            IM_COL32(255, 255, 255, 180),
            IM_COL32(100, 130, 165, 140),
            IM_COL32(50, 75, 110, 150),
            3.5f);

        // Progress bar 2: Total progress (Soft Vibrant Aqua Crystal Bevel with smooth animation)
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 154));
        RenderBeveledProgressBar(
            animTotalProgress_,
            ImVec2(362, 14),
            IM_COL32(170, 245, 255, 150),
            IM_COL32(22, 150, 218, 255),
            IM_COL32(255, 255, 255, 190),
            IM_COL32(8, 80, 135, 150),
            IM_COL32(10, 75, 125, 160),
            4.0f);

        // Status text & download speed
        ImGui::SetCursorPos(ImVec2(23, winSize.y - 185));
        ImGui::Text("%s", state.fileString.c_str());

        if (!state.speedString.empty())
        {
            ImVec2 textSize = ImGui::CalcTextSize(state.speedString.c_str());
            ImGui::SetCursorPos(ImVec2(winSize.x - textSize.x - 24, winSize.y - 185));
            ImGui::Text("%s", state.speedString.c_str());
        }

        // Website link footer
        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 76));
        ImGui::Text("%s", lang::GetString("launcher_site_desc").c_str());

        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 60));
        RenderLink(
            lang::GetString("launcher_site_click").c_str(),
            config::WebsiteLink.c_str(),
            events.onLinkClicked);

        // Button: Check Files
        ImGui::SetCursorPos(ImVec2(23, winSize.y - 132));
        const bool checkLocked = !state.isCheckEnabled;
        if (ImageButton(
                lang::GetString("launcher_check").c_str(),
                textures.CheckNormal(),
                textures.CheckHover(),
                textures.CheckSelected(),
                textures.CheckGray(),
                ImVec2(113, 27),
                checkLocked))
        {
            if (!checkLocked && events.onCheckClicked)
                events.onCheckClicked();
        }

        // Button: Options
        ImGui::SetCursorPos(ImVec2(150, winSize.y - 132));
        const bool optionLocked = !state.isOptionEnabled;
        if (ImageButton(
                lang::GetString("launcher_options").c_str(),
                textures.OptionNormal(),
                textures.OptionHover(),
                textures.OptionSelected(),
                textures.OptionGray(),
                ImVec2(113, 27),
                optionLocked))
        {
            if (!optionLocked && events.onOptionClicked)
                events.onOptionClicked();
        }

        // Button: Exit
        ImGui::SetCursorPos(ImVec2(274, winSize.y - 132));
        if (ImageButton(
                lang::GetString("launcher_exit").c_str(),
                textures.ExitNormal(),
                textures.ExitHover(),
                textures.ExitSelected(),
                textures.ExitSelected(),
                ImVec2(113, 27),
                false))
        {
            if (events.onExitClicked)
                events.onExitClicked();
            else
                shouldClose_ = true;
        }

        // Button: Game Start
        ImGui::SetCursorPos(ImVec2(winSize.x - 139, winSize.y - 166));
        const bool gameLocked = !state.isGameEnabled;
        if (ImageButton(
                lang::GetString("launcher_game_start").c_str(),
                textures.GameNormal(),
                textures.GameHover(),
                textures.GameSelected(),
                textures.GameGray(),
                ImVec2(118, 61),
                gameLocked))
        {
            if (!gameLocked && events.onPlayClicked)
                events.onPlayClicked();
        }

        ImGui::End();
    }

    if (!showWindow)
        shouldClose_ = true;
}
