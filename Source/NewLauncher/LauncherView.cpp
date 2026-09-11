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
    style.Colors[ImGuiCol_Text] = ImVec4(0.06f, 0.09f, 0.16f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.92f, 0.96f, 0.99f, 1.0f);
    style.WindowRounding = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0, 0);

    char windowsDir[MAX_PATH]{};
    GetWindowsDirectoryA(windowsDir, MAX_PATH);
    std::string fontBoldPath = std::string(windowsDir) + "\\Fonts\\segoeuib.ttf";
    std::string fontRegPath = std::string(windowsDir) + "\\Fonts\\segoeui.ttf";

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    const float smallFontSize = 13.0f;
    const float baseFontSize = 14.5f;
    const float boldFontSize = 16.5f;
    const float largeFontSize = 19.0f;

    auto LoadFont = [&](const char* path, float size) -> ImFont*
    {
        ImFontConfig fontCfg;
        fontCfg.MergeMode = false;
        return io.Fonts->AddFontFromFileTTF(path, size, &fontCfg, glyphRanges);
    };

    fontSmall_ = LoadFont(fontBoldPath.c_str(), smallFontSize);
    if (!fontSmall_)
        fontSmall_ = LoadFont(fontRegPath.c_str(), smallFontSize);

    fontRegular_ = LoadFont(fontBoldPath.c_str(), baseFontSize);
    if (!fontRegular_)
        fontRegular_ = LoadFont(fontRegPath.c_str(), baseFontSize);

    fontBold_ = LoadFont(fontBoldPath.c_str(), boldFontSize);
    if (!fontBold_)
        fontBold_ = LoadFont(fontRegPath.c_str(), boldFontSize);

    fontLarge_ = LoadFont(fontBoldPath.c_str(), largeFontSize);
    if (!fontLarge_)
        fontLarge_ = LoadFont(fontRegPath.c_str(), largeFontSize);

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

    const float rounding = (std::min)(size.y * 0.5f, 18.0f); // Full smooth pill shape
    const float t = static_cast<float>(ImGui::GetTime());
    const bool isPrimaryCTA = (accentColor == IM_COL32(37, 99, 235, 255)) || pulseGlow;

    if (isLocked)
    {
        // High-contrast muted slate pill (clear and easily readable disabled state)
        dl->AddRectFilled(p0, p1, IM_COL32(230, 236, 243, 255), rounding);
        dl->AddRect(p0, p1, IM_COL32(180, 196, 214, 255), rounding, 0, 1.2f);

        if (label && *label)
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            ImVec2 textPos = ImVec2(
                p0.x + (size.x - textSize.x) * 0.5f,
                p0.y + (size.y - textSize.y) * 0.5f);
            dl->AddText(textPos, IM_COL32(51, 65, 85, 255), label); // #334155 - Crisp dark slate text!
        }
    }
    else if (isPrimaryCTA)
    {
        // ── Primary Call-To-Action Button (Royal Blue Pill like website เข้าสู่ระบบ) ──
        if (pulseGlow)
        {
            const float pulseAlpha = (sinf(t * 3.5f) * 0.5f + 0.5f) * 90.0f + 50.0f;
            dl->AddRect(
                ImVec2(p0.x - 3.0f, p0.y - 3.0f),
                ImVec2(p1.x + 3.0f, p1.y + 3.0f),
                IM_COL32(37, 99, 235, static_cast<int>(pulseAlpha)),
                rounding + 3.0f,
                0,
                2.0f);
        }

        // Soft outer blue drop-shadow
        dl->AddRectFilled(
            ImVec2(p0.x + 1.0f, p0.y + 2.0f),
            ImVec2(p1.x - 1.0f, p1.y + 4.5f),
            IM_COL32(30, 64, 175, 60),
            rounding);

        // Gradient Fill
        ImU32 colTop, colBottom;
        if (held)
        {
            colTop = IM_COL32(30, 64, 175, 255);    // #1e40af
            colBottom = IM_COL32(29, 78, 216, 255); // #1d4ed8
        }
        else if (hovered)
        {
            colTop = IM_COL32(59, 130, 246, 255);   // #3b82f6
            colBottom = IM_COL32(37, 99, 235, 255); // #2563eb
        }
        else
        {
            colTop = IM_COL32(37, 99, 235, 255);    // #2563eb
            colBottom = IM_COL32(29, 78, 216, 255); // #1d4ed8
        }

        dl->AddRectFilledMultiColor(p0, p1, colTop, colTop, colBottom, colBottom);

        // Soft top-half specular sheen
        dl->AddRectFilled(
            p0,
            ImVec2(p1.x, p0.y + size.y * 0.45f),
            IM_COL32(255, 255, 255, hovered ? 70 : 45),
            rounding,
            ImDrawFlags_RoundCornersTop);

        // Luminous border
        dl->AddRect(p0, p1, IM_COL32(191, 219, 254, 240), rounding, 0, 1.2f);

        // Crisp White Typography with soft depth
        if (label && *label)
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            const float yOffset = held ? 1.0f : 0.0f;
            ImVec2 textPos = ImVec2(
                p0.x + (size.x - textSize.x) * 0.5f,
                p0.y + (size.y - textSize.y) * 0.5f + yOffset);

            dl->AddText(ImVec2(textPos.x + 0.5f, textPos.y + 1.0f), IM_COL32(15, 23, 42, 180), label);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
        }
    }
    else
    {
        // ── Secondary Floating White Pill Buttons (Check, Options, Exit) ──
        const bool isExit = (accentColor == IM_COL32(239, 68, 68, 255));

        // Soft ambient card shadow
        dl->AddRectFilled(
            ImVec2(p0.x, p0.y + 1.0f),
            ImVec2(p1.x, p1.y + 3.0f),
            IM_COL32(50, 80, 120, 35),
            rounding);

        // Base fill & Crisp Border
        ImU32 baseFill = IM_COL32(255, 255, 255, 255);
        ImU32 borderCol = IM_COL32(148, 163, 184, 255); // #94a3b8 - Crisp clear border!

        if (held)
        {
            baseFill = isExit ? IM_COL32(254, 226, 226, 255) : IM_COL32(224, 242, 254, 255);
            borderCol = isExit ? IM_COL32(248, 113, 113, 255) : IM_COL32(37, 99, 235, 255);
        }
        else if (hovered)
        {
            baseFill = isExit ? IM_COL32(255, 241, 242, 255) : IM_COL32(239, 246, 255, 255);
            borderCol = isExit ? IM_COL32(225, 29, 72, 255) : IM_COL32(37, 99, 235, 255);
        }

        dl->AddRectFilled(p0, p1, baseFill, rounding);
        dl->AddRect(p0, p1, borderCol, rounding, 0, 1.3f);

        // Typography (Deep Navy with high contrast and legibility)
        if (label && *label)
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            const float yOffset = held ? 1.0f : 0.0f;
            ImVec2 textPos = ImVec2(
                p0.x + (size.x - textSize.x) * 0.5f,
                p0.y + (size.y - textSize.y) * 0.5f + yOffset);

            ImU32 textCol = isExit && hovered
                ? IM_COL32(225, 29, 72, 255)
                : IM_COL32(15, 23, 42, 255); // #0f172a - High contrast bold text!

            dl->AddText(textPos, textCol, label);
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
    return ModernButton(id, fallbackLabel ? fallbackLabel : id, size, isLocked, fallbackAccent);
}

void LauncherView::RenderLink(
    const char* label,
    const char* url,
    const std::function<void(const std::string&)>& onClick) noexcept
{
    // Trickster Classic Amber/Orange Accent with crisp bold contrast
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(217, 119, 6, 255));
    if (ImGui::Text("%s", label); ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 p0 = ImGui::GetItemRectMin();
        ImVec2 p1 = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p0.x, p1.y - 1.0f),
            ImVec2(p1.x, p1.y - 1.0f),
            IM_COL32(217, 119, 6, 255),
            1.2f);

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

    const float pillRounding = (std::min)(size.y * 0.5f, rounding);

    // 1. Light Sky-Blue Sunken Capsule Track
    dl->AddRectFilled(
        ImVec2(p0.x - 0.5f, p0.y - 0.5f),
        ImVec2(p1.x + 0.5f, p1.y + 0.5f),
        IM_COL32(203, 213, 225, 120),
        pillRounding + 0.5f);
    dl->AddRectFilled(p0, p1, IM_COL32(224, 242, 254, 240), pillRounding); // #e0f2fe

    // Subtle track border
    dl->AddRect(p0, p1, IM_COL32(147, 197, 253, 240), pillRounding, 0, 1.0f); // #93c5fd

    // 2. Filled Progress Capsule
    const float f = (std::clamp)(fraction, 0.0f, 1.0f);
    const float pad = 1.0f;
    const ImVec2 barP0 = ImVec2(p0.x + pad, p0.y + pad);
    const float maxBarW = size.x - pad * 2.0f;
    const float barH = size.y - pad * 2.0f;
    const float barW = maxBarW * f;

    if (barW >= 2.0f)
    {
        const ImVec2 barP1 = ImVec2(barP0.x + barW, barP0.y + barH);
        const float barRounding = (std::min)(pillRounding - 0.5f, barW * 0.5f);
        const ImDrawFlags cornerFlags = (barW >= maxBarW - 1.0f)
            ? ImDrawFlags_RoundCornersAll
            : ImDrawFlags_RoundCornersLeft;

        // Base Gradient Fill (Royal Blue & Cerulean)
        dl->AddRectFilledMultiColor(barP0, barP1, colTop, colTop, colBottom, colBottom);

        // Soft Layered Top Sheen
        const ImDrawFlags topCornerFlags = (cornerFlags & ImDrawFlags_RoundCornersLeft)
            ? (ImDrawFlags_RoundCornersTopLeft | (barW >= maxBarW - 1.0f ? ImDrawFlags_RoundCornersTopRight : 0))
            : ImDrawFlags_None;

        dl->AddRectFilled(
            barP0,
            ImVec2(barP1.x, barP0.y + barH * 0.5f),
            IM_COL32(255, 255, 255, 75),
            barRounding,
            topCornerFlags);

        // Animated diagonal wave shimmer gliding across
        const float t = static_cast<float>(ImGui::GetTime());
        const float skewX = barH * 0.75f;
        const float shimmerPhase = fmodf(t * 1.15f, 1.8f) - 0.4f;
        const float shimmerCenter = barP0.x + (maxBarW + skewX * 2.0f) * shimmerPhase - skewX;
        const float shimmerWidth = 24.0f;
        const float coreWidth = 10.0f;

        dl->PushClipRect(barP0, barP1, true);

        // Soft outer diagonal beam
        dl->AddQuadFilled(
            ImVec2(shimmerCenter + skewX - shimmerWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter + skewX + shimmerWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter - skewX + shimmerWidth * 0.5f, barP1.y),
            ImVec2(shimmerCenter - skewX - shimmerWidth * 0.5f, barP1.y),
            IM_COL32(255, 255, 255, 60));

        // Bright inner diagonal core beam
        dl->AddQuadFilled(
            ImVec2(shimmerCenter + skewX - coreWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter + skewX + coreWidth * 0.5f, barP0.y),
            ImVec2(shimmerCenter - skewX + coreWidth * 0.5f, barP1.y),
            ImVec2(shimmerCenter - skewX - coreWidth * 0.5f, barP1.y),
            IM_COL32(255, 255, 255, 110));

        dl->PopClipRect();

        // Soft pulse on the leading edge
        if (f > 0.02f && f < 0.999f)
        {
            const float pulseAlpha = (sinf(t * 5.0f) * 0.5f + 0.5f) * 50.0f + 30.0f;
            dl->AddRectFilled(
                ImVec2(barP1.x - 3.0f, barP0.y),
                barP1,
                IM_COL32(255, 255, 255, static_cast<int>(pulseAlpha)),
                barRounding,
                cornerFlags);
        }

        // Fill border
        dl->AddRect(barP0, barP1, colBorder, barRounding, cornerFlags, 1.0f);
    }

    if (showPercentage && size.y >= 12.0f)
    {
        char pctBuf[16]{};
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(f * 100.0f + 0.5f));
        ImVec2 pctSize = ImGui::CalcTextSize(pctBuf);
        ImVec2 pctPos = ImVec2(p0.x + (size.x - pctSize.x) * 0.5f, p0.y + (size.y - pctSize.y) * 0.5f);

        // High contrast percentage with subtle shadow for perfect legibility
        dl->AddText(ImVec2(pctPos.x + 0.5f, pctPos.y + 1.0f), IM_COL32(15, 23, 42, 180), pctBuf);
        dl->AddText(pctPos, IM_COL32(255, 255, 255, 255), pctBuf);
    }

    // Advance ImGui cursor
    ImGui::Dummy(size);
}

void LauncherView::Render(
    const LauncherViewState& state,
    const TextureManager& textures,
    const LauncherViewEvents& events) noexcept
{
    (void)textures; // Modern theme bypasses legacy bitmaps
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

        // 1. Soft Sky Blue & Cloud Pastel Backdrop (Matching Trickster Classic Website Atmosphere)
        dl->AddRectFilledMultiColor(
            ImVec2(0, 0),
            winSize,
            IM_COL32(220, 238, 253, 255), // Top Left: Light Baby Sky Blue (#dbeefd)
            IM_COL32(238, 247, 255, 255), // Top Right: Soft Cloud Mist (#eef7ff)
            IM_COL32(204, 232, 252, 255), // Bottom Right: Crisp Aqua Sky (#cce8fc)
            IM_COL32(224, 242, 254, 255)  // Bottom Left: Soft Sky Mist (#e0f2fe)
        );

        // Window Perimeter Border with delicate sky blue stroke
        dl->AddRect(ImVec2(0, 0), winSize, IM_COL32(186, 215, 243, 230), 16.0f, 0, 1.2f);

        // 2. Custom Clean Title Bar (0..34px Y - Translucent White Cloud Glass)
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(winSize.x, 34), IM_COL32(255, 255, 255, 230), 16.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(0, 34), ImVec2(winSize.x, 34), IM_COL32(203, 213, 225, 220), 1.0f);

        // Brand Title Badge ("Trickster Classic")
        ImGui::SetCursorPos(ImVec2(16, 7));
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(ImVec4(0.06f, 0.16f, 0.32f, 1.0f), "Trickster");
        ImGui::SameLine(0, 5.0f);
        ImGui::TextColored(ImVec4(0.85f, 0.40f, 0.02f, 1.0f), "Classic");
        if (fontBold_) ImGui::PopFont();

        // Server Status Pill Badge (Light Blue Capsule like website "ศูนย์ข่าวเซิร์ฟเวอร์")
        const float t = static_cast<float>(ImGui::GetTime());
        const bool isMaint = state.fileString.find("manuten") != std::string::npos ||
                             state.fileString.find("Maintenance") != std::string::npos;
        const char* statusText = isMaint ? "MAINTENANCE" : "ONLINE";
        ImVec2 stSize = ImGui::CalcTextSize(statusText);
        const float pillW = stSize.x + 24.0f;
        const float pillX = winSize.x - 72.0f - pillW;

        dl->AddRectFilled(
            ImVec2(pillX, 6),
            ImVec2(pillX + pillW, 28),
            isMaint ? IM_COL32(254, 226, 226, 245) : IM_COL32(219, 234, 254, 245),
            11.0f);
        dl->AddRect(
            ImVec2(pillX, 6),
            ImVec2(pillX + pillW, 28),
            isMaint ? IM_COL32(252, 165, 165, 240) : IM_COL32(96, 165, 250, 240),
            11.0f,
            0,
            1.0f);

        // Glowing indicator circle
        const float pulseDot = (sinf(t * 4.0f) * 0.5f + 0.5f) * 55.0f + 200.0f;
        ImU32 dotCol = isMaint
            ? IM_COL32(239, 68, 68, static_cast<int>(pulseDot))
            : IM_COL32(16, 185, 129, static_cast<int>(pulseDot));
        dl->AddCircleFilled(ImVec2(pillX + 9, 17), 3.5f, dotCol);

        if (fontSmall_) ImGui::PushFont(fontSmall_);
        dl->AddText(
            ImVec2(pillX + 17, 9.0f),
            isMaint ? IM_COL32(220, 38, 38, 255) : IM_COL32(29, 78, 216, 255),
            statusText);
        if (fontSmall_) ImGui::PopFont();

        // Minimize Button ("-")
        ImGui::SetCursorPos(ImVec2(winSize.x - 64, 6));
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
            dl->AddRectFilled(minP0, minP1, IM_COL32(226, 232, 240, 240), 6.0f);
        dl->AddLine(ImVec2(minP0.x + 7, minP0.y + 11), ImVec2(minP1.x - 7, minP0.y + 11), IM_COL32(51, 65, 85, 240), 1.8f);
        ImGui::PopID();

        // Close Button ("✕")
        ImGui::SetCursorPos(ImVec2(winSize.x - 34, 6));
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
            dl->AddRectFilled(closeP0, closeP1, IM_COL32(244, 63, 94, 240), 6.0f);
        dl->AddLine(
            ImVec2(closeP0.x + 8, closeP0.y + 6),
            ImVec2(closeP1.x - 8, closeP1.y - 6),
            closeHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(51, 65, 85, 240),
            1.8f);
        dl->AddLine(
            ImVec2(closeP1.x - 8, closeP0.y + 6),
            ImVec2(closeP0.x + 8, closeP1.y - 6),
            closeHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(51, 65, 85, 240),
            1.8f);
        ImGui::PopID();

        // 3. News WebView Floating Card Frame (16, 40, 522, 376)
        dl->AddRectFilled(
            ImVec2(17.0f, 41.0f),
            ImVec2(523.0f, 377.0f),
            IM_COL32(30, 70, 120, 20),
            12.0f);
        dl->AddRect(
            ImVec2(17.0f, 40.0f),
            ImVec2(522.0f, 376.0f),
            IM_COL32(186, 215, 243, 255),
            12.0f,
            0,
            1.2f);

        // 4. Bottom Floating Card (Housing Progress, Telemetry and Controls)
        const float bottomCardY = 384.0f;
        const float bottomCardH = winSize.y - bottomCardY - 10.0f;
        dl->AddRectFilled(
            ImVec2(14.0f, bottomCardY + 1.0f),
            ImVec2(winSize.x - 14.0f, bottomCardY + bottomCardH + 1.0f),
            IM_COL32(30, 70, 120, 25),
            16.0f);
        dl->AddRectFilled(
            ImVec2(14.0f, bottomCardY),
            ImVec2(winSize.x - 14.0f, bottomCardY + bottomCardH),
            IM_COL32(255, 255, 255, 255),
            16.0f);
        dl->AddRect(
            ImVec2(14.0f, bottomCardY),
            ImVec2(winSize.x - 14.0f, bottomCardY + bottomCardH),
            IM_COL32(186, 215, 243, 255),
            16.0f,
            0,
            1.2f);

        // Status text & download speed chip
        ImGui::SetCursorPos(ImVec2(28, bottomCardY + 10));
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(ImVec4(0.06f, 0.09f, 0.16f, 1.0f), "%s", state.fileString.c_str());
        if (fontBold_) ImGui::PopFont();

        if (!state.speedString.empty())
        {
            ImVec2 textSize = ImGui::CalcTextSize(state.speedString.c_str());
            const float chipW = textSize.x + 16.0f;
            const float chipX = winSize.x - chipW - 28.0f;
            const float chipY = bottomCardY + 8.0f;

            // Speed Chip Pill (Light Sky Blue Pill)
            dl->AddRectFilled(
                ImVec2(chipX, chipY),
                ImVec2(chipX + chipW, chipY + 20),
                IM_COL32(219, 234, 254, 245),
                10.0f);
            dl->AddRect(
                ImVec2(chipX, chipY),
                ImVec2(chipX + chipW, chipY + 20),
                IM_COL32(96, 165, 250, 240),
                10.0f,
                0,
                1.0f);

            ImGui::SetCursorPos(ImVec2(chipX + 8, chipY + 2.5f));
            if (fontSmall_) ImGui::PushFont(fontSmall_);
            ImGui::TextColored(ImVec4(0.11f, 0.31f, 0.85f, 1.0f), "%s", state.speedString.c_str());
            if (fontSmall_) ImGui::PopFont();
        }

        // Dual Progress Bars
        // Progress bar 1: File progress (Soft Sky-Blue Capsule)
        ImGui::SetCursorPos(ImVec2(28, bottomCardY + 32));
        RenderBeveledProgressBar(
            animFileProgress_,
            ImVec2(348, 8),
            IM_COL32(147, 197, 253, 255), // #93c5fd
            IM_COL32(96, 165, 250, 255),  // #60a5fa
            IM_COL32(255, 255, 255, 160),
            IM_COL32(59, 130, 246, 100),
            IM_COL32(37, 99, 235, 140),
            4.0f,
            false);

        // Progress bar 2: Total progress (Vibrant Cerulean/Royal Blue Capsule with Percentage)
        ImGui::SetCursorPos(ImVec2(28, bottomCardY + 44));
        RenderBeveledProgressBar(
            animTotalProgress_,
            ImVec2(348, 14),
            IM_COL32(56, 189, 248, 255),  // #38bdf8
            IM_COL32(37, 99, 235, 255),   // #2563eb
            IM_COL32(255, 255, 255, 180),
            IM_COL32(29, 78, 216, 120),
            IM_COL32(30, 64, 175, 150),
            7.0f,
            true);

        // Primary Action: Big Game Start Button (Vibrant Royal Blue Pill like website เข้าสู่ระบบ)
        ImGui::SetCursorPos(ImVec2(winSize.x - 146, bottomCardY + 26));
        const bool gameLocked = !state.isGameEnabled;
        if (fontLarge_) ImGui::PushFont(fontLarge_);
        if (ModernButton(
                "##play_btn",
                lang::GetString("launcher_game_start").c_str(),
                ImVec2(118, 52),
                gameLocked,
                IM_COL32(37, 99, 235, 255),
                !gameLocked))
        {
            if (!gameLocked && events.onPlayClicked)
                events.onPlayClicked();
        }
        if (fontLarge_) ImGui::PopFont();

        // Secondary Action Buttons (Check, Options, Exit) - Floating White Pill Buttons
        const float btnY = bottomCardY + 68.0f;
        const float btnW = 108.0f;
        const float btnH = 28.0f;

        if (fontBold_) ImGui::PushFont(fontBold_);

        // Button: Check Files
        ImGui::SetCursorPos(ImVec2(28, btnY));
        const bool checkLocked = !state.isCheckEnabled;
        if (ModernButton(
                "##btn_check",
                lang::GetString("launcher_check").c_str(),
                ImVec2(btnW, btnH),
                checkLocked))
        {
            if (!checkLocked && events.onCheckClicked)
                events.onCheckClicked();
        }

        // Button: Options
        ImGui::SetCursorPos(ImVec2(146, btnY));
        const bool optionLocked = !state.isOptionEnabled;
        if (ModernButton(
                "##btn_options",
                lang::GetString("launcher_options").c_str(),
                ImVec2(btnW, btnH),
                optionLocked))
        {
            if (!optionLocked && events.onOptionClicked)
                events.onOptionClicked();
        }

        // Button: Exit
        ImGui::SetCursorPos(ImVec2(264, btnY));
        if (ModernButton(
                "##btn_exit",
                lang::GetString("launcher_exit").c_str(),
                ImVec2(btnW, btnH),
                false,
                IM_COL32(239, 68, 68, 255)))
        {
            if (events.onExitClicked)
                events.onExitClicked();
            else
                shouldClose_ = true;
        }

        if (fontBold_) ImGui::PopFont();

        // 5. Footer: Website Link
        ImGui::SetCursorPos(ImVec2(28, bottomCardY + 106));
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(ImVec4(0.12f, 0.16f, 0.23f, 1.0f), "%s", lang::GetString("launcher_site_desc").c_str());

        ImGui::SameLine(0, 6.0f);
        RenderLink(
            lang::GetString("launcher_site_click").c_str(),
            config::WebsiteLink.c_str(),
            events.onLinkClicked);
        if (fontRegular_) ImGui::PopFont();

        ImGui::End();
    }

    if (!showWindow)
        shouldClose_ = true;
}
