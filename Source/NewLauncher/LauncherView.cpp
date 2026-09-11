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
    style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.97f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.12f, 1.0f);
    style.WindowRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0, 0);

    char windowsDir[MAX_PATH]{};
    GetWindowsDirectoryA(windowsDir, MAX_PATH);
    std::string fontPath = std::string(windowsDir) + "\\Fonts\\segoeuib.ttf";
    std::string fontRegPath = std::string(windowsDir) + "\\Fonts\\segoeui.ttf";

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    const float baseFontSize = 15.0f;
    const float smallFontSize = 12.0f;
    const float boldFontSize = 16.0f;

    auto LoadFont = [&](const char* path, float size) -> ImFont*
    {
        ImFontConfig fontCfg;
        fontCfg.MergeMode = false;
        return io.Fonts->AddFontFromFileTTF(path, size, &fontCfg, glyphRanges);
    };

    fontSmall_ = LoadFont(fontRegPath.c_str(), smallFontSize);
    if (!fontSmall_)
        fontSmall_ = LoadFont(fontPath.c_str(), smallFontSize);

    fontRegular_ = LoadFont(fontRegPath.c_str(), baseFontSize);
    if (!fontRegular_)
        fontRegular_ = LoadFont(fontPath.c_str(), baseFontSize);

    fontBold_ = LoadFont(fontPath.c_str(), boldFontSize);

    io.FontDefault = fontRegular_ ? fontRegular_ : (io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0]);
    io.Fonts->Build();
    io.IniFilename = nullptr;
}

bool LauncherView::ModernButton(
    const char* id,
    const char* label,
    const ImVec2& size,
    bool isLocked,
    ImU32 accentColor,
    bool pulseGlow) noexcept
{
    ImGui::PushID(id);
    bool pressedResult = ImGui::InvisibleButton("##mbtn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rounding = 5.0f;
    const float t = static_cast<float>(ImGui::GetTime());

    if (isLocked)
    {
        // Darkened muted slate button
        dl->AddRectFilled(p0, p1, IM_COL32(24, 30, 42, 220), rounding);
        dl->AddRect(p0, p1, IM_COL32(45, 55, 75, 140), rounding, 0, 1.0f);

        if (label && *label)
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            ImVec2 textPos = ImVec2(
                p0.x + (size.x - textSize.x) * 0.5f,
                p0.y + (size.y - textSize.y) * 0.5f);
            dl->AddText(textPos, IM_COL32(110, 125, 145, 180), label);
        }
    }
    else
    {
        // Ambient glow when pulsing (e.g. Game Start button ready)
        if (pulseGlow)
        {
            const float pulseAlpha = (sinf(t * 3.5f) * 0.5f + 0.5f) * 70.0f + 40.0f;
            ImU32 glowCol = (accentColor & 0x00FFFFFF) | (static_cast<ImU32>(pulseAlpha) << 24);
            dl->AddRect(
                ImVec2(p0.x - 2.0f, p0.y - 2.0f),
                ImVec2(p1.x + 2.0f, p1.y + 2.0f),
                glowCol,
                rounding + 2.0f,
                0,
                2.0f);
        }

        // Soft outer drop-shadow
        dl->AddRectFilled(
            ImVec2(p0.x, p0.y + 1.5f),
            ImVec2(p1.x, p1.y + 2.5f),
            IM_COL32(0, 0, 0, 80),
            rounding);

        // Gradient base fill
        ImU32 colTop, colBottom;
        if (held)
        {
            colTop = IM_COL32(20, 70, 110, 255);
            colBottom = IM_COL32(10, 45, 80, 255);
        }
        else if (hovered)
        {
            colTop = IM_COL32(35, 120, 190, 255);
            colBottom = IM_COL32(18, 75, 130, 255);
        }
        else
        {
            colTop = IM_COL32(26, 85, 140, 240);
            colBottom = IM_COL32(14, 52, 95, 240);
        }

        dl->AddRectFilledMultiColor(p0, p1, colTop, colTop, colBottom, colBottom);

        // Top specular gloss line
        dl->AddLine(
            ImVec2(p0.x + rounding, p0.y + 1.0f),
            ImVec2(p1.x - rounding, p0.y + 1.0f),
            hovered ? IM_COL32(255, 255, 255, 160) : IM_COL32(255, 255, 255, 90),
            1.0f);

        // Border
        ImU32 borderCol = hovered ? IM_COL32(80, 200, 255, 220) : IM_COL32(40, 130, 200, 170);
        dl->AddRect(p0, p1, borderCol, rounding, 0, 1.0f);

        // Centered typography with soft shadow
        if (label && *label)
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            const float yOffset = held ? 1.0f : 0.0f;
            ImVec2 textPos = ImVec2(
                p0.x + (size.x - textSize.x) * 0.5f,
                p0.y + (size.y - textSize.y) * 0.5f + yOffset);

            dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 160), label);
            dl->AddText(textPos, hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(230, 242, 255, 245), label);
        }
    }

    ImGui::PopID();
    return pressedResult && !isLocked;
}

bool LauncherView::ImageButton(
    const char* id,
    IDirect3DTexture9* normal,
    IDirect3DTexture9* hover,
    IDirect3DTexture9* pressed,
    IDirect3DTexture9* locked,
    const ImVec2& size,
    bool isLocked,
    const char* fallbackLabel,
    ImU32 fallbackAccent) noexcept
{
    // If bitmap textures are missing, render the high-end procedural vector button
    if (!normal && !locked)
    {
        return ModernButton(id, fallbackLabel ? fallbackLabel : id, size, isLocked, fallbackAccent);
    }

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
    {
        // Smooth hover drop-shadow / soft glow behind sprite
        if (hovered && !isLocked)
        {
            dl->AddRectFilled(
                ImVec2(min.x + 1.0f, min.y + 1.0f),
                ImVec2(max.x - 1.0f, max.y - 1.0f),
                IM_COL32(0, 180, 255, 25),
                4.0f);
        }
        dl->AddImage(tex, min, max);
    }
    else
    {
        // Secondary fallback
        ModernButton(id, fallbackLabel ? fallbackLabel : id, size, isLocked, fallbackAccent);
    }

    ImGui::PopID();
    return pressedResult;
}

void LauncherView::RenderLink(
    const char* label,
    const char* url,
    const std::function<void(const std::string&)>& onClick) noexcept
{
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 115, 255));
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
    float rounding,
    bool showPercentage) noexcept
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Inset background track (soft outer shadow + sunken slot)
    dl->AddRectFilled(ImVec2(p0.x - 1.0f, p0.y - 1.0f), ImVec2(p1.x + 1.0f, p1.y + 1.0f), IM_COL32(0, 0, 0, 70), rounding + 1.0f);
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

        // Soft layered bevel gloss dome
        const ImDrawFlags topCornerFlags = (cornerFlags & ImDrawFlags_RoundCornersLeft)
            ? (ImDrawFlags_RoundCornersTopLeft | (barW >= maxBarW - 1.0f ? ImDrawFlags_RoundCornersTopRight : 0))
            : ImDrawFlags_None;

        // Layer 1: Broad soft upper sheen
        dl->AddRectFilled(barP0, ImVec2(barP1.x, barP0.y + barH * 0.52f), colTop, barRounding, topCornerFlags);

        // Layer 2: Subtle top-quarter highlight for rounded dome curve
        ImU32 colTopCrest = (colTop & 0x00FFFFFF) | (static_cast<ImU32>((colTop >> 24) * 0.7f) << 24);
        dl->AddRectFilled(barP0, ImVec2(barP1.x, barP0.y + barH * 0.28f), colTopCrest, barRounding, topCornerFlags);

        // Animated diagonal wave shimmer gliding smoothly and briskly across the bar
        const float t = static_cast<float>(ImGui::GetTime());
        const float skewX = barH * 0.75f;
        const float shimmerPhase = fmodf(t * 1.15f, 1.8f) - 0.4f;
        const float shimmerCenter = barP0.x + (maxBarW + skewX * 2.0f) * shimmerPhase - skewX;
        const float shimmerWidth = 24.0f;
        const float coreWidth = 10.0f;

        // Clip the diagonal sheen cleanly to the progress bar's filled bounds
        dl->PushClipRect(barP0, barP1, true);

        // Soft outer diagonal beam
        dl->AddQuadFilled(
            ImVec2(shimmerCenter + skewX - shimmerWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter + skewX + shimmerWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter - skewX + shimmerWidth * 0.5f, barP1.y),
            ImVec2(shimmerCenter - skewX - shimmerWidth * 0.5f, barP1.y),
            IM_COL32(255, 255, 255, 24));

        // Bright inner diagonal core beam
        dl->AddQuadFilled(
            ImVec2(shimmerCenter + skewX - coreWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter + skewX + coreWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter - skewX + coreWidth * 0.5f, barP1.y),
            ImVec2(shimmerCenter - skewX - coreWidth * 0.5f, barP1.y),
            IM_COL32(255, 255, 255, 48));

        dl->PopClipRect();

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

    if (showPercentage && size.y >= 12.0f)
    {
        char pctBuf[16]{};
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(f * 100.0f + 0.5f));
        ImVec2 pctSize = ImGui::CalcTextSize(pctBuf);
        ImVec2 pctPos = ImVec2(p0.x + (size.x - pctSize.x) * 0.5f, p0.y + (size.y - pctSize.y) * 0.5f);
        dl->AddText(ImVec2(pctPos.x + 1.0f, pctPos.y + 1.0f), IM_COL32(0, 0, 0, 180), pctBuf);
        dl->AddText(pctPos, IM_COL32(255, 255, 255, 240), pctBuf);
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
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winSize = ImGui::GetWindowSize();

        // 1. Window Base Background (Deep Modern Slate Gradient + Neon Accent Edge)
        dl->AddRectFilledMultiColor(
            ImVec2(0, 0),
            winSize,
            IM_COL32(11, 15, 24, 255),
            IM_COL32(14, 20, 32, 255),
            IM_COL32(18, 25, 40, 255),
            IM_COL32(13, 18, 30, 255));

        // If background texture exists, draw it over the base
        if (textures.Background())
        {
            dl->AddImage(
                reinterpret_cast<ImTextureID>(textures.Background()),
                ImVec2(0, 0),
                winSize);
        }

        // Modern glowing perimeter frame
        dl->AddRect(ImVec2(0, 0), winSize, IM_COL32(45, 75, 110, 180), 0.0f, 0, 1.0f);
        dl->AddLine(ImVec2(0, 1), ImVec2(winSize.x, 1), IM_COL32(0, 200, 255, 100), 1.0f);

        // 2. Custom Modern Title Bar (0..30px Y)
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(winSize.x, 30), IM_COL32(8, 12, 20, 220));
        dl->AddLine(ImVec2(0, 30), ImVec2(winSize.x, 30), IM_COL32(30, 48, 72, 180), 1.0f);

        // Title text / Brand
        ImGui::SetCursorPos(ImVec2(14, 6));
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(ImVec4(0.85f, 0.93f, 1.0f, 1.0f), "%s", config::SubTitle.c_str());
        if (fontBold_) ImGui::PopFont();

        // Server Status Pill Badge (Pulsing Green dot)
        const float t = static_cast<float>(ImGui::GetTime());
        const bool isMaint = state.fileString.find("manuten") != std::string::npos ||
                             state.fileString.find("Maintenance") != std::string::npos;
        const char* statusText = isMaint ? "MAINTENANCE" : "ONLINE";
        ImVec2 stSize = ImGui::CalcTextSize(statusText);
        const float pillW = stSize.x + 24.0f;
        const float pillX = winSize.x - 72.0f - pillW;

        dl->AddRectFilled(ImVec2(pillX, 5), ImVec2(pillX + pillW, 25), isMaint ? IM_COL32(239, 68, 68, 30) : IM_COL32(16, 185, 129, 30), 10.0f);
        dl->AddRect(ImVec2(pillX, 5), ImVec2(pillX + pillW, 25), isMaint ? IM_COL32(239, 68, 68, 120) : IM_COL32(16, 185, 129, 120), 10.0f, 0, 1.0f);

        // Glowing indicator circle
        const float pulseDot = (sinf(t * 4.0f) * 0.5f + 0.5f) * 60.0f + 195.0f;
        ImU32 dotCol = isMaint ? IM_COL32(239, 68, 68, static_cast<int>(pulseDot)) : IM_COL32(16, 185, 129, static_cast<int>(pulseDot));
        dl->AddCircleFilled(ImVec2(pillX + 9, 15), 3.5f, dotCol);

        if (fontSmall_) ImGui::PushFont(fontSmall_);
        dl->AddText(ImVec2(pillX + 17, 7.5f), isMaint ? IM_COL32(248, 113, 113, 240) : IM_COL32(110, 231, 183, 240), statusText);
        if (fontSmall_) ImGui::PopFont();

        // Minimize Button ("-")
        ImGui::SetCursorPos(ImVec2(winSize.x - 64, 4));
        ImGui::PushID("##title_min");
        if (ImGui::InvisibleButton("##btn_min", ImVec2(26, 22)))
        {
            if (events.onMinimizeClicked)
                events.onMinimizeClicked();
        }
        bool minHovered = ImGui::IsItemHovered();
        ImVec2 minP0 = ImGui::GetItemRectMin();
        ImVec2 minP1 = ImGui::GetItemRectMax();
        if (minHovered)
            dl->AddRectFilled(minP0, minP1, IM_COL32(255, 255, 255, 30), 4.0f);
        dl->AddLine(ImVec2(minP0.x + 7, minP0.y + 11), ImVec2(minP1.x - 7, minP0.y + 11), IM_COL32(210, 225, 245, 220), 1.5f);
        ImGui::PopID();

        // Close Button ("✕")
        ImGui::SetCursorPos(ImVec2(winSize.x - 34, 4));
        ImGui::PushID("##title_close");
        if (ImGui::InvisibleButton("##btn_close", ImVec2(26, 22)))
        {
            if (events.onExitClicked)
                events.onExitClicked();
            else
                shouldClose_ = true;
        }
        bool closeHovered = ImGui::IsItemHovered();
        ImVec2 closeP0 = ImGui::GetItemRectMin();
        ImVec2 closeP1 = ImGui::GetItemRectMax();
        if (closeHovered)
            dl->AddRectFilled(closeP0, closeP1, IM_COL32(225, 29, 72, 220), 4.0f);
        dl->AddLine(ImVec2(closeP0.x + 8, closeP0.y + 6), ImVec2(closeP1.x - 8, closeP1.y - 6), IM_COL32(240, 240, 240, 240), 1.5f);
        dl->AddLine(ImVec2(closeP1.x - 8, closeP0.y + 6), ImVec2(closeP0.x + 8, closeP1.y - 6), IM_COL32(240, 240, 240, 240), 1.5f);
        ImGui::PopID();

        // 3. News WebView Card Frame (18, 31, 523, 376)
        dl->AddRect(
            ImVec2(18.0f, 31.0f),
            ImVec2(523.0f, 376.0f),
            IM_COL32(40, 60, 88, 160),
            4.0f,
            0,
            1.0f);

        // Logo
        if (textures.Logo())
        {
            ImGui::SetCursorPos(ImVec2(11, winSize.y - 98));
            ImGui::Image(
                reinterpret_cast<ImTextureID>(textures.Logo()),
                ImVec2(185, 71));
        }

        // 4. Progress Bars Area
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
            3.5f,
            false);

        // Progress bar 2: Total progress (Soft Vibrant Aqua Crystal Bevel with percentage indicator)
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 154));
        RenderBeveledProgressBar(
            animTotalProgress_,
            ImVec2(362, 14),
            IM_COL32(170, 245, 255, 150),
            IM_COL32(22, 150, 218, 255),
            IM_COL32(255, 255, 255, 190),
            IM_COL32(8, 80, 135, 150),
            IM_COL32(10, 75, 125, 160),
            4.0f,
            true);

        // Status text & download speed chip
        ImGui::SetCursorPos(ImVec2(23, winSize.y - 186));
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(ImVec4(0.85f, 0.92f, 1.0f, 0.9f), "%s", state.fileString.c_str());

        if (!state.speedString.empty())
        {
            ImVec2 textSize = ImGui::CalcTextSize(state.speedString.c_str());
            const float chipW = textSize.x + 14.0f;
            const float chipX = winSize.x - chipW - 24.0f;
            const float chipY = winSize.y - 188.0f;

            dl->AddRectFilled(ImVec2(chipX, chipY), ImVec2(chipX + chipW, chipY + 18), IM_COL32(0, 162, 237, 35), 4.0f);
            dl->AddRect(ImVec2(chipX, chipY), ImVec2(chipX + chipW, chipY + 18), IM_COL32(0, 162, 237, 120), 4.0f, 0, 1.0f);

            ImGui::SetCursorPos(ImVec2(chipX + 7, chipY + 2));
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", state.speedString.c_str());
        }
        if (fontSmall_) ImGui::PopFont();

        // Website link footer
        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 76));
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(ImVec4(0.7f, 0.78f, 0.88f, 0.85f), "%s", lang::GetString("launcher_site_desc").c_str());

        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 60));
        RenderLink(
            lang::GetString("launcher_site_click").c_str(),
            config::WebsiteLink.c_str(),
            events.onLinkClicked);
        if (fontSmall_) ImGui::PopFont();

        // 5. Action Buttons (Check, Options, Exit)
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
                checkLocked,
                lang::GetString("launcher_check").c_str()))
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
                optionLocked,
                lang::GetString("launcher_options").c_str()))
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
                false,
                lang::GetString("launcher_exit").c_str(),
                IM_COL32(239, 68, 68, 255)))
        {
            if (events.onExitClicked)
                events.onExitClicked();
            else
                shouldClose_ = true;
        }

        // 6. Big Game Start Button
        ImGui::SetCursorPos(ImVec2(winSize.x - 139, winSize.y - 166));
        const bool gameLocked = !state.isGameEnabled;
        if (textures.GameNormal() || textures.GameGray())
        {
            // If sprite exists, check if we should add energy pulse when ready
            if (!gameLocked)
            {
                const float pulseAlpha = (sinf(t * 4.0f) * 0.5f + 0.5f) * 60.0f + 25.0f;
                dl->AddRect(
                    ImVec2(winSize.x - 141, winSize.y - 168),
                    ImVec2(winSize.x - 19, winSize.y - 103),
                    IM_COL32(0, 210, 255, static_cast<int>(pulseAlpha)),
                    6.0f,
                    0,
                    2.0f);
            }

            if (ImageButton(
                    lang::GetString("launcher_game_start").c_str(),
                    textures.GameNormal(),
                    textures.GameHover(),
                    textures.GameSelected(),
                    textures.GameGray(),
                    ImVec2(118, 61),
                    gameLocked,
                    lang::GetString("launcher_game_start").c_str()))
            {
                if (!gameLocked && events.onPlayClicked)
                    events.onPlayClicked();
            }
        }
        else
        {
            // Vector procedural Play button
            if (ModernButton(
                    "##play_btn",
                    lang::GetString("launcher_game_start").c_str(),
                    ImVec2(118, 61),
                    gameLocked,
                    IM_COL32(0, 200, 255, 255),
                    !gameLocked))
            {
                if (!gameLocked && events.onPlayClicked)
                    events.onPlayClicked();
            }
        }

        ImGui::End();
    }

    if (!showWindow)
        shouldClose_ = true;
}
