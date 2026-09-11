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

static inline ImU32 BlendCol(ImU32 c1, ImU32 c2, float t) noexcept
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    int r1 = (c1 >> IM_COL32_R_SHIFT) & 0xFF;
    int g1 = (c1 >> IM_COL32_G_SHIFT) & 0xFF;
    int b1 = (c1 >> IM_COL32_B_SHIFT) & 0xFF;
    int a1 = (c1 >> IM_COL32_A_SHIFT) & 0xFF;

    int r2 = (c2 >> IM_COL32_R_SHIFT) & 0xFF;
    int g2 = (c2 >> IM_COL32_G_SHIFT) & 0xFF;
    int b2 = (c2 >> IM_COL32_B_SHIFT) & 0xFF;
    int a2 = (c2 >> IM_COL32_A_SHIFT) & 0xFF;

    int r = static_cast<int>(r1 + (r2 - r1) * t);
    int g = static_cast<int>(g1 + (g2 - g1) * t);
    int b = static_cast<int>(b1 + (b2 - b1) * t);
    int a = static_cast<int>(a1 + (a2 - a1) * t);

    return IM_COL32(r, g, b, a);
}

void LauncherView::RenderButtonIcon(
    ButtonIcon icon,
    const ImVec2& center,
    float size,
    ImU32 color,
    ImDrawList* dl) noexcept
{
    if (icon == ButtonIcon::None || !dl)
        return;

    const float r = size * 0.5f;

    switch (icon)
    {
    case ButtonIcon::Play:
    {
        // Stylized crisp play triangle pointing right
        ImVec2 p0(center.x - r * 0.45f, center.y - r * 0.70f);
        ImVec2 p1(center.x + r * 0.85f, center.y);
        ImVec2 p2(center.x - r * 0.45f, center.y + r * 0.70f);
        dl->AddTriangleFilled(p0, p1, p2, color);
        break;
    }
    case ButtonIcon::Check:
    {
        // Stylized checkmark with crisp lines
        ImVec2 p0(center.x - r * 0.65f, center.y);
        ImVec2 p1(center.x - r * 0.15f, center.y + r * 0.55f);
        ImVec2 p2(center.x + r * 0.75f, center.y - r * 0.55f);
        dl->AddLine(p0, p1, color, 1.8f);
        dl->AddLine(p1, p2, color, 1.8f);
        break;
    }
    case ButtonIcon::Settings:
    {
        // Minimalist modern gear: outer ring + core + 4 tick rays
        dl->AddCircle(center, r * 0.70f, color, 12, 1.5f);
        dl->AddCircleFilled(center, r * 0.20f, color);
        for (int i = 0; i < 4; ++i)
        {
            const float ang = static_cast<float>(i) * (3.14159265f / 2.0f);
            const float cosA = cosf(ang);
            const float sinA = sinf(ang);
            dl->AddLine(
                ImVec2(center.x + cosA * (r * 0.55f), center.y + sinA * (r * 0.55f)),
                ImVec2(center.x + cosA * (r * 0.95f), center.y + sinA * (r * 0.95f)),
                color,
                1.5f);
        }
        break;
    }
    case ButtonIcon::Exit:
    {
        // Minimalist power icon: circle arc + vertical line
        dl->AddCircle(center, r * 0.70f, color, 12, 1.5f);
        dl->AddLine(
            ImVec2(center.x, center.y - r * 0.90f),
            ImVec2(center.x, center.y - r * 0.15f),
            color,
            1.8f);
        break;
    }
    default:
        break;
    }
}

bool LauncherView::ModernButton(
    const char* id,
    const char* label,
    const ImVec2& size,
    bool isLocked,
    ButtonIcon icon,
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

    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    // Smooth hover lerp animation
    float& hoverVal = buttonHover_[id];
    const float targetHover = (hovered && !isLocked) ? 1.0f : 0.0f;
    hoverVal += (targetHover - hoverVal) * (1.0f - expf(-18.0f * dt));

    const float rounding = size.y * 0.5f; // Full smooth rounded pill capsule
    const float t = static_cast<float>(ImGui::GetTime());
    const float yOffset = (held && !isLocked) ? 1.2f : 0.0f;

    // Icon & Label geometry
    const float iconSize = (icon != ButtonIcon::None) ? ((icon == ButtonIcon::Play) ? 15.0f : 12.0f) : 0.0f;
    const float iconSpacing = (icon != ButtonIcon::None && label && *label) ? 6.0f : 0.0f;
    ImVec2 textSize = (label && *label) ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    const float totalContentW = iconSize + iconSpacing + textSize.x;
    const float startX = p0.x + (size.x - totalContentW) * 0.5f;
    const float centerY = p0.y + size.y * 0.5f + yOffset;

    if (isLocked)
    {
        // Neutral muted gray pill (distinctly disabled, easily readable without looking active)
        dl->AddRectFilled(p0, p1, IM_COL32(241, 245, 249, 255), rounding);
        dl->AddRect(p0, p1, IM_COL32(203, 213, 225, 255), rounding, 0, 1.0f);

        const ImU32 lockedCol = IM_COL32(100, 116, 139, 255); // #64748b - Slate-500 neutral gray

        if (icon != ButtonIcon::None)
        {
            RenderButtonIcon(icon, ImVec2(startX + iconSize * 0.5f, centerY), iconSize, lockedCol, dl);
        }

        if (label && *label)
        {
            ImVec2 textPos(
                (icon != ButtonIcon::None) ? (startX + iconSize + iconSpacing) : (p0.x + (size.x - textSize.x) * 0.5f),
                p0.y + (size.y - textSize.y) * 0.5f);
            dl->AddText(textPos, lockedCol, label);
        }
    }
    else
    {
        // ── Floating Clean Pill Buttons (Matching Trickster Classic Theme) ──
        ImVec2 drawP0 = ImVec2(p0.x, p0.y + yOffset);
        ImVec2 drawP1 = ImVec2(p1.x, p1.y + yOffset);

        // Idle breathing pulse aura for primary CTA (Game Start / Connect)
        if (pulseGlow && hoverVal <= 0.01f && !held)
        {
            const float pulse = (sinf(t * 3.5f) * 0.5f + 0.5f) * 40.0f + 30.0f;
            dl->AddRect(
                ImVec2(drawP0.x - 2.5f, drawP0.y - 2.5f),
                ImVec2(drawP1.x + 2.5f, drawP1.y + 2.5f),
                IM_COL32(249, 115, 22, static_cast<int>(pulse)),
                rounding + 2.5f,
                0,
                1.5f);
        }

        // Luminous warm orange glow on hover
        if (hoverVal > 0.01f && !held)
        {
            const float pulse = (sinf(t * 4.5f) * 0.5f + 0.5f) * 35.0f + 65.0f;
            const int glowAlpha = static_cast<int>(pulse * hoverVal);
            dl->AddRect(
                ImVec2(drawP0.x - 3.0f, drawP0.y - 3.0f),
                ImVec2(drawP1.x + 3.0f, drawP1.y + 3.0f),
                IM_COL32(249, 115, 22, glowAlpha),
                rounding + 3.0f,
                0,
                2.0f);
        }

        // Tactile ambient shadow
        const float shadowH = (held) ? 1.0f : ((size.y > 40.0f) ? 4.0f : 3.0f);
        dl->AddRectFilled(
            ImVec2(p0.x, p0.y + 1.0f),
            ImVec2(p1.x, p1.y + shadowH),
            IM_COL32(50, 80, 120, 35),
            rounding);

        // Smoothly animated Base fill & Border with Trickster Orange Palette
        const ImU32 normFill = IM_COL32(255, 255, 255, 255);
        const ImU32 hovFill = IM_COL32(255, 247, 237, 255); // #fff7ed
        const ImU32 normBorder = pulseGlow ? IM_COL32(249, 115, 22, 180) : IM_COL32(148, 163, 184, 255);
        const ImU32 hovBorder = IM_COL32(249, 115, 22, 255);  // #f97316

        ImU32 baseFill = BlendCol(normFill, hovFill, hoverVal);
        ImU32 borderCol = BlendCol(normBorder, hovBorder, hoverVal);

        if (held)
        {
            baseFill = IM_COL32(255, 237, 213, 255);  // #ffedd5
            borderCol = IM_COL32(234, 88, 12, 255);   // #ea580c
        }

        dl->AddRectFilled(drawP0, drawP1, baseFill, rounding);
        dl->AddRect(drawP0, drawP1, borderCol, rounding, 0, (size.y > 40.0f ? 1.8f : 1.3f) + 0.3f * hoverVal);

        // Diagonal shimmer sweep reflection on hover or primary CTA
        if ((hoverVal > 0.05f || pulseGlow) && !held)
        {
            const float shimmerFactor = hoverVal > 0.05f ? hoverVal : 0.4f;
            dl->PushClipRect(drawP0, drawP1, true);
            const float skewX = size.y * 0.65f;
            const float sweepSpeed = hoverVal > 0.05f ? 1.5f : 0.8f;
            const float sweepPhase = fmodf(t * sweepSpeed, 2.0f) - 0.5f;
            const float sweepCenter = drawP0.x + (size.x + skewX * 2.0f) * sweepPhase - skewX;
            const int shimmerAlpha = static_cast<int>(90.0f * shimmerFactor);
            const int coreAlpha = static_cast<int>(150.0f * shimmerFactor);

            // Soft outer beam
            dl->AddQuadFilled(
                ImVec2(sweepCenter + skewX - 16.0f, drawP0.y),
                ImVec2(sweepCenter + skewX + 16.0f, drawP0.y),
                ImVec2(sweepCenter - skewX + 16.0f, drawP1.y),
                ImVec2(sweepCenter - skewX - 16.0f, drawP1.y),
                IM_COL32(255, 247, 237, shimmerAlpha));

            // Bright core beam
            dl->AddQuadFilled(
                ImVec2(sweepCenter + skewX - 6.0f, drawP0.y),
                ImVec2(sweepCenter + skewX + 6.0f, drawP0.y),
                ImVec2(sweepCenter - skewX + 6.0f, drawP1.y),
                ImVec2(sweepCenter - skewX - 6.0f, drawP1.y),
                IM_COL32(255, 255, 255, coreAlpha));

            dl->PopClipRect();
        }

        // Smoothly animated Typography and Icon (Deep Navy normal ➔ Trickster Orange on hover)
        const ImU32 normText = IM_COL32(15, 23, 42, 255);
        const ImU32 hovText = IM_COL32(234, 88, 12, 255); // #ea580c
        ImU32 textCol = BlendCol(normText, hovText, hoverVal);

        if (icon != ButtonIcon::None)
        {
            RenderButtonIcon(icon, ImVec2(startX + iconSize * 0.5f, centerY), iconSize, textCol, dl);
        }

        if (label && *label)
        {
            ImVec2 textPos(
                (icon != ButtonIcon::None) ? (startX + iconSize + iconSpacing) : (p0.x + (size.x - textSize.x) * 0.5f),
                p0.y + (size.y - textSize.y) * 0.5f + yOffset);

            dl->AddText(textPos, textCol, label);
        }
    }

    ImGui::PopID();
    return pressedResult && !isLocked;
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

void LauncherView::SetSavedAccount(const std::string& account, bool remember) noexcept
{
    rememberAccount_ = remember;
    if (!account.empty())
    {
        strncpy_s(accountBuffer_, account.c_str(), sizeof(accountBuffer_) - 1);
    }
}

void LauncherView::RenderLoginForm(
    float bottomCardY,
    const ImVec2& winSize,
    const LauncherViewEvents& events) noexcept
{
    // 1. Header / Status line
    ImGui::SetCursorPos(ImVec2(28, bottomCardY + 8));
    if (fontBold_) ImGui::PushFont(fontBold_);
    if (!authErrorText_.empty())
    {
        ImGui::TextColored(ImVec4(0.88f, 0.20f, 0.20f, 1.0f), "%s", authErrorText_.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.06f, 0.09f, 0.16f, 1.0f), "%s", lang::GetString("launcher_login_account").c_str());
    }
    if (fontBold_) ImGui::PopFont();

    // 2. Custom Input Styles for Modern Pastel Theme
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.2f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(248, 250, 252, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(241, 245, 249, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(255, 255, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(186, 215, 243, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(15, 23, 42, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(148, 163, 184, 255));

    if (fontRegular_) ImGui::PushFont(fontRegular_);

    // Account ID Input
    ImGui::SetCursorPos(ImVec2(28, bottomCardY + 28));
    ImGui::SetNextItemWidth(210.0f);
    bool enterAccount = ImGui::InputTextWithHint(
        "##account_input",
        lang::GetString("launcher_login_account").c_str(),
        accountBuffer_,
        sizeof(accountBuffer_),
        ImGuiInputTextFlags_EnterReturnsTrue);

    // Password Input
    ImGui::SetCursorPos(ImVec2(28, bottomCardY + 58));
    ImGui::SetNextItemWidth(210.0f);
    bool enterPass = ImGui::InputTextWithHint(
        "##password_input",
        lang::GetString("launcher_login_password").c_str(),
        passwordBuffer_,
        sizeof(passwordBuffer_),
        ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

    if (fontRegular_) ImGui::PopFont();

    // Remember ID Checkbox
    ImGui::SetCursorPos(ImVec2(28, bottomCardY + 88));
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(37, 99, 235, 255));
    ImGui::Checkbox(lang::GetString("launcher_login_remember").c_str(), &rememberAccount_);
    ImGui::PopStyleColor();
    if (fontSmall_) ImGui::PopFont();

    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    // 3. BACK Button
    ImGui::SetCursorPos(ImVec2(248, bottomCardY + 44));
    if (fontBold_) ImGui::PushFont(fontBold_);
    if (ModernButton(
            "##btn_back",
            lang::GetString("launcher_login_back").c_str(),
            ImVec2(96, 32),
            false,
            ButtonIcon::Exit))
    {
        showLoginForm_ = false;
        authErrorText_.clear();
    }
    if (fontBold_) ImGui::PopFont();

    // 4. Primary CONNECT Button
    ImGui::SetCursorPos(ImVec2(winSize.x - 146, bottomCardY + 32));
    if (fontLarge_) ImGui::PushFont(fontLarge_);
    bool connectClicked = ModernButton(
        "##connect_btn",
        lang::GetString("launcher_login_connect").c_str(),
        ImVec2(118, 58),
        false,
        ButtonIcon::Play,
        IM_COL32(37, 99, 235, 255),
        true);
    if (fontLarge_) ImGui::PopFont();

    if (connectClicked || enterAccount || enterPass)
    {
        if (events.onConnectClicked)
        {
            events.onConnectClicked(accountBuffer_, passwordBuffer_, rememberAccount_);
        }
    }

    // 5. Footer: Website Link
    ImGui::SetCursorPos(ImVec2(28, bottomCardY + 114));
    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(ImVec4(0.12f, 0.16f, 0.23f, 1.0f), "%s", lang::GetString("launcher_site_desc").c_str());

    ImGui::SameLine(0, 6.0f);
    RenderLink(
        lang::GetString("launcher_site_click").c_str(),
        config::WebsiteLink.c_str(),
        events.onLinkClicked);
    if (fontRegular_) ImGui::PopFont();
}

void LauncherView::Render(
    const LauncherViewState& state,
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

    ImGui::Begin(
        "##MainWindow",
        nullptr,
        nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

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

        // Logo Icon & Title
        const char* titleText = "Trickster Online";
        if (fontBold_) ImGui::PushFont(fontBold_);
        dl->AddText(ImVec2(16, 8), IM_COL32(15, 23, 42, 255), titleText);
        if (fontBold_) ImGui::PopFont();

        // Server Status Tag Pill
        const bool isMaint = (state.fileString == lang::GetString("launcher_worker_maintenance"));
        const char* statusText = isMaint ? "Maintenance" : "Server Online";
        const float pillX = 142.0f;

        // Pill background
        dl->AddRectFilled(
            ImVec2(pillX, 6),
            ImVec2(pillX + 104, 28),
            isMaint ? IM_COL32(254, 226, 226, 240) : IM_COL32(238, 242, 255, 240),
            11.0f);
        dl->AddRect(
            ImVec2(pillX, 6),
            ImVec2(pillX + 104, 28),
            isMaint ? IM_COL32(248, 113, 113, 220) : IM_COL32(165, 180, 252, 220),
            11.0f,
            0,
            1.0f);

        // Glowing indicator circle
        const float t = static_cast<float>(ImGui::GetTime());
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

        // 3. News WebView Floating Card Frame (16, 38, 522, 368)
        dl->AddRectFilled(
            ImVec2(17.0f, 39.0f),
            ImVec2(523.0f, 369.0f),
            IM_COL32(30, 70, 120, 20),
            12.0f);
        dl->AddRect(
            ImVec2(17.0f, 38.0f),
            ImVec2(522.0f, 368.0f),
            IM_COL32(186, 215, 243, 255),
            12.0f,
            0,
            1.2f);

        // 4. Bottom Floating Card (Housing Progress, Telemetry, Controls or Login Form)
        const float bottomCardY = 376.0f;
        const float bottomCardH = winSize.y - bottomCardY - 8.0f;
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

        if (showLoginForm_)
        {
            RenderLoginForm(bottomCardY, winSize, events);
        }
        else
        {
            // Status text & download speed chip
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 8));
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(ImVec4(0.06f, 0.09f, 0.16f, 1.0f), "%s", state.fileString.c_str());
            if (fontBold_) ImGui::PopFont();

            if (!state.speedString.empty())
            {
                ImVec2 textSize = ImGui::CalcTextSize(state.speedString.c_str());
                const float chipW = textSize.x + 16.0f;
                const float chipX = winSize.x - chipW - 28.0f;
                const float chipY = bottomCardY + 6.0f;

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
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 28));
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
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 40));
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

            // Primary Action: Big Game Start Button (Transitions to Login Form on click)
            ImGui::SetCursorPos(ImVec2(winSize.x - 146, bottomCardY + 32));
            const bool gameLocked = !state.isGameEnabled;
            if (fontLarge_) ImGui::PushFont(fontLarge_);
            if (ModernButton(
                    "##play_btn",
                    lang::GetString("launcher_game_start").c_str(),
                    ImVec2(118, 58),
                    gameLocked,
                    ButtonIcon::Play,
                    IM_COL32(37, 99, 235, 255),
                    !gameLocked))
            {
                if (!gameLocked)
                {
                    showLoginForm_ = true;
                    if (events.onPlayClicked)
                        events.onPlayClicked();
                }
            }
            if (fontLarge_) ImGui::PopFont();

            // Secondary Action Buttons (Check, Options, Exit) - Floating White Pill Buttons
            const float btnY = bottomCardY + 62.0f;
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
                    checkLocked,
                    ButtonIcon::Check))
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
                    optionLocked,
                    ButtonIcon::Settings))
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
                    ButtonIcon::Exit))
            {
                if (events.onExitClicked)
                    events.onExitClicked();
                else
                    shouldClose_ = true;
            }

            if (fontBold_) ImGui::PopFont();

            // 5. Footer: Website Link
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 98));
            if (fontRegular_) ImGui::PushFont(fontRegular_);
            ImGui::TextColored(ImVec4(0.12f, 0.16f, 0.23f, 1.0f), "%s", lang::GetString("launcher_site_desc").c_str());

            ImGui::SameLine(0, 6.0f);
            RenderLink(
                lang::GetString("launcher_site_click").c_str(),
                config::WebsiteLink.c_str(),
                events.onLinkClicked);
            if (fontRegular_) ImGui::PopFont();
        }

        ImGui::End();
}
