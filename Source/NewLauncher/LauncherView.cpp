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

#include <d3d9.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <filesystem>
#include <random>
#include <cctype>
#include "Config.h"
#include "Language.h"
#include "Logger.h"

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
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesDefault();
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

    const float rounding = (std::min)(size.y * 0.5f, 18.0f); // Full smooth pill shape
    const float t = static_cast<float>(ImGui::GetTime());
    const bool isPrimaryCTA = (accentColor == IM_COL32(37, 99, 235, 255)) || pulseGlow;
    const float yOffset = (held && !isLocked) ? 1.2f : 0.0f;

    // Icon & Label geometry
    const float iconSize = (icon != ButtonIcon::None) ? ((icon == ButtonIcon::Play) ? 14.0f : 12.0f) : 0.0f;
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

        // Tactile depth shadow
        const float shadowH = (held) ? 1.5f : 4.5f;
        dl->AddRectFilled(
            ImVec2(p0.x + 1.0f, p0.y + 2.0f),
            ImVec2(p1.x - 1.0f, p1.y + shadowH),
            IM_COL32(30, 64, 175, 60),
            rounding);

        // Gradient Fill (smoothly blended on hover)
        const ImU32 baseTop = IM_COL32(37, 99, 235, 255);   // #2563eb
        const ImU32 baseBottom = IM_COL32(29, 78, 216, 255); // #1d4ed8
        const ImU32 hoverTop = IM_COL32(59, 130, 246, 255);  // #3b82f6
        const ImU32 hoverBottom = IM_COL32(37, 99, 235, 255);

        ImU32 colTop = BlendCol(baseTop, hoverTop, hoverVal);
        ImU32 colBottom = BlendCol(baseBottom, hoverBottom, hoverVal);

        if (held)
        {
            colTop = IM_COL32(30, 64, 175, 255);
            colBottom = IM_COL32(29, 78, 216, 255);
        }

        ImVec2 drawP0 = ImVec2(p0.x, p0.y + yOffset);
        ImVec2 drawP1 = ImVec2(p1.x, p1.y + yOffset);

        // Gradient Fill — two rounded rects stacked (AddRectFilledMultiColor ignores rounding)
        dl->AddRectFilled(drawP0, drawP1, colBottom, rounding);
        dl->AddRectFilled(drawP0,
            ImVec2(drawP1.x, drawP0.y + size.y * 0.5f),
            colTop, rounding, ImDrawFlags_RoundCornersTop);


        // Soft top-half specular sheen
        const int sheenAlpha = static_cast<int>(45.0f + 25.0f * hoverVal);
        dl->AddRectFilled(
            drawP0,
            ImVec2(drawP1.x, drawP0.y + size.y * 0.45f),
            IM_COL32(255, 255, 255, sheenAlpha),
            rounding,
            ImDrawFlags_RoundCornersTop);

        // Luminous border
        dl->AddRect(drawP0, drawP1, IM_COL32(191, 219, 254, 240), rounding, 0, 1.2f);

        // Diagonal shimmer sweep glide
        if (!held)
        {
            dl->PushClipRect(drawP0, drawP1, true);
            const float skewX = size.y * 0.6f;
            const float sweepPhase = fmodf(t * 0.9f, 2.2f) - 0.5f;
            const float sweepCenter = drawP0.x + (size.x + skewX * 2.0f) * sweepPhase - skewX;
            dl->AddQuadFilled(
                ImVec2(sweepCenter + skewX - 16.0f, drawP0.y),
                ImVec2(sweepCenter + skewX + 16.0f, drawP0.y),
                ImVec2(sweepCenter - skewX + 16.0f, drawP1.y),
                ImVec2(sweepCenter - skewX - 16.0f, drawP1.y),
                IM_COL32(255, 255, 255, 45));
            dl->PopClipRect();
        }

        // Crisp White Typography & Play Icon with soft depth
        if (icon != ButtonIcon::None)
        {
            RenderButtonIcon(icon, ImVec2(startX + iconSize * 0.5f, centerY + 1.0f), iconSize, IM_COL32(15, 23, 42, 160), dl);
            RenderButtonIcon(icon, ImVec2(startX + iconSize * 0.5f, centerY), iconSize, IM_COL32(255, 255, 255, 255), dl);
        }

        if (label && *label)
        {
            ImVec2 textPos(
                (icon != ButtonIcon::None) ? (startX + iconSize + iconSpacing) : (p0.x + (size.x - textSize.x) * 0.5f),
                p0.y + (size.y - textSize.y) * 0.5f + yOffset);

            dl->AddText(ImVec2(textPos.x + 0.5f, textPos.y + 1.0f), IM_COL32(15, 23, 42, 180), label);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
        }
    }
    else
    {
        // ── Secondary Floating White Pill Buttons (Check, Options, Exit) ──
        ImVec2 drawP0 = ImVec2(p0.x, p0.y + yOffset);
        ImVec2 drawP1 = ImVec2(p1.x, p1.y + yOffset);

        // Luminous warm orange glow on hover
        if (hoverVal > 0.01f && !held)
        {
            const float pulse = (sinf(t * 4.5f) * 0.5f + 0.5f) * 35.0f + 65.0f;
            const int glowAlpha = static_cast<int>(pulse * hoverVal);
            dl->AddRect(
                ImVec2(drawP0.x - 2.5f, drawP0.y - 2.5f),
                ImVec2(drawP1.x + 2.5f, drawP1.y + 2.5f),
                IM_COL32(249, 115, 22, glowAlpha),
                rounding + 2.5f,
                0,
                1.8f);
        }

        // Tactile ambient shadow
        const float shadowH = (held) ? 1.0f : 3.0f;
        dl->AddRectFilled(
            ImVec2(p0.x, p0.y + 1.0f),
            ImVec2(p1.x, p1.y + shadowH),
            IM_COL32(50, 80, 120, 35),
            rounding);

        // Smoothly animated Base fill & Border with Trickster Orange Palette
        const ImU32 normFill = IM_COL32(255, 255, 255, 255);
        const ImU32 hovFill = IM_COL32(255, 247, 237, 255); // #fff7ed
        const ImU32 normBorder = IM_COL32(148, 163, 184, 255); // #94a3b8
        const ImU32 hovBorder = IM_COL32(249, 115, 22, 255);  // #f97316

        ImU32 baseFill = BlendCol(normFill, hovFill, hoverVal);
        ImU32 borderCol = BlendCol(normBorder, hovBorder, hoverVal);

        if (held)
        {
            baseFill = IM_COL32(255, 237, 213, 255);  // #ffedd5
            borderCol = IM_COL32(234, 88, 12, 255);   // #ea580c
        }

        dl->AddRectFilled(drawP0, drawP1, baseFill, rounding);
        dl->AddRect(drawP0, drawP1, borderCol, rounding, 0, 1.3f + 0.3f * hoverVal);

        // Diagonal shimmer sweep reflection on hover
        if (hoverVal > 0.05f && !held)
        {
            dl->PushClipRect(drawP0, drawP1, true);
            const float skewX = size.y * 0.65f;
            const float sweepPhase = fmodf(t * 1.5f, 1.8f) - 0.4f;
            const float sweepCenter = drawP0.x + (size.x + skewX * 2.0f) * sweepPhase - skewX;
            const int shimmerAlpha = static_cast<int>(90.0f * hoverVal);
            const int coreAlpha = static_cast<int>(150.0f * hoverVal);

            // Soft outer beam
            dl->AddQuadFilled(
                ImVec2(sweepCenter + skewX - 14.0f, drawP0.y),
                ImVec2(sweepCenter + skewX + 14.0f, drawP0.y),
                ImVec2(sweepCenter - skewX + 14.0f, drawP1.y),
                ImVec2(sweepCenter - skewX - 14.0f, drawP1.y),
                IM_COL32(255, 247, 237, shimmerAlpha));

            // Bright core beam
            dl->AddQuadFilled(
                ImVec2(sweepCenter + skewX - 5.0f, drawP0.y),
                ImVec2(sweepCenter + skewX + 5.0f, drawP0.y),
                ImVec2(sweepCenter - skewX + 5.0f, drawP1.y),
                ImVec2(sweepCenter - skewX - 5.0f, drawP1.y),
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

static void RenderGear(
    ImDrawList* dl,
    const ImVec2& center,
    float radius,
    int teeth,
    ImU32 bodyCol,
    ImU32 borderCol,
    ImU32 centerCol,
    float angle) noexcept
{
    if (!dl || radius <= 2.0f) return;

    const float toothDepth = radius * 0.35f;
    const float innerR = radius - toothDepth;
    const float holeR = innerR * 0.40f;
    const float step = (3.14159265f * 2.0f) / static_cast<float>(teeth);

    // Draw teeth & outer body using polyline and convex fill
    std::vector<ImVec2> pts;
    pts.reserve(teeth * 4);
    for (int i = 0; i < teeth; ++i)
    {
        const float a0 = angle + i * step;
        const float a1 = a0 + step * 0.32f;
        const float a2 = a0 + step * 0.50f;
        const float a3 = a0 + step * 0.82f;

        pts.push_back(ImVec2(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius));
        pts.push_back(ImVec2(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius));
        pts.push_back(ImVec2(center.x + cosf(a2) * innerR, center.y + sinf(a2) * innerR));
        pts.push_back(ImVec2(center.x + cosf(a3) * innerR, center.y + sinf(a3) * innerR));
    }

    if (!pts.empty())
    {
        dl->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), bodyCol);
        dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), borderCol, ImDrawFlags_Closed, 1.2f);
    }

    // Inner core
    dl->AddCircleFilled(center, innerR * 0.70f, bodyCol);
    dl->AddCircle(center, innerR * 0.70f, borderCol, 12, 1.0f);

    // Axle hole
    dl->AddCircleFilled(center, holeR, centerCol);
    dl->AddCircle(center, holeR, borderCol, 8, 1.0f);
}

static void RenderSparkleStar(ImDrawList* dl, ImVec2 pos, float size, ImU32 color, float alpha) noexcept
{
    if (!dl || size <= 0.0f) return;
    const int r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    const int g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    const int b = (color >> IM_COL32_B_SHIFT) & 0xFF;
    const ImU32 c = IM_COL32(r, g, b, static_cast<int>(alpha * 255.0f));

    // 4-point diamond star
    const ImVec2 pT(pos.x, pos.y - size);
    const ImVec2 pR(pos.x + size * 0.55f, pos.y);
    const ImVec2 pB(pos.x, pos.y + size);
    const ImVec2 pL(pos.x - size * 0.55f, pos.y);

    dl->AddQuadFilled(pT, pR, pB, pL, c);
    dl->AddCircleFilled(pos, size * 0.25f, IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0f)));
}

static void RenderMascotDrillIndicator(
    ImDrawList* dl,
    const ImVec2& tipPos,
    float time,
    bool isDownloading) noexcept
{
    if (!dl) return;

    // Drilling vibration jitter while active
    const float jitterY = isDownloading ? (sinf(time * 35.0f) * 0.7f) : 0.0f;
    const float jitterX = isDownloading ? (cosf(time * 28.0f) * 0.35f) : 0.0f;
    const ImVec2 baseTip(tipPos.x + jitterX, tipPos.y + jitterY);

    // 1. Floating sparkling stars around the mascot (Cyan, Gold, Pink, White)
    const float s1Alpha = sinf(time * 6.0f) * 0.35f + 0.65f;
    const float s2Alpha = cosf(time * 5.0f + 1.2f) * 0.35f + 0.65f;
    const float s3Alpha = sinf(time * 7.0f + 2.5f) * 0.35f + 0.65f;

    RenderSparkleStar(dl, ImVec2(baseTip.x - 14.0f, baseTip.y - 22.0f + sinf(time * 3.0f) * 2.0f), 4.5f, IM_COL32(56, 189, 248, 255), s1Alpha);
    RenderSparkleStar(dl, ImVec2(baseTip.x + 14.0f, baseTip.y - 24.0f + cosf(time * 3.5f) * 2.0f), 4.0f, IM_COL32(251, 191, 36, 255), s2Alpha);
    RenderSparkleStar(dl, ImVec2(baseTip.x - 16.0f, baseTip.y - 10.0f + sinf(time * 4.0f) * 1.5f), 3.5f, IM_COL32(244, 114, 182, 255), s3Alpha);
    RenderSparkleStar(dl, ImVec2(baseTip.x + 16.0f, baseTip.y - 12.0f + cosf(time * 4.5f) * 1.5f), 3.5f, IM_COL32(255, 255, 255, 255), s1Alpha);

    // 2. Metallic Spiral Drill Bit pointing directly at tipPos
    const ImVec2 bitTip(baseTip.x, baseTip.y + 1.0f);
    const ImVec2 bitL(baseTip.x - 5.5f, baseTip.y - 11.0f);
    const ImVec2 bitR(baseTip.x + 5.5f, baseTip.y - 11.0f);

    // Drill cone fill & border
    dl->AddTriangleFilled(bitTip, bitL, bitR, IM_COL32(226, 232, 240, 255));
    dl->AddTriangle(bitTip, bitL, bitR, IM_COL32(51, 65, 85, 255), 1.2f);

    // Spiral groove lines across the drill bit
    const float spiralPhase = fmodf(time * 12.0f, 1.0f);
    for (int i = 0; i < 3; ++i)
    {
        const float tY = (static_cast<float>(i) + spiralPhase) / 3.0f;
        if (tY >= 0.15f && tY <= 0.90f)
        {
            const float y = baseTip.y - tY * 10.0f;
            const float w = (1.0f - tY) * 4.5f + 1.0f;
            dl->AddLine(
                ImVec2(baseTip.x - w, y),
                ImVec2(baseTip.x + w, y - 1.5f),
                IM_COL32(71, 85, 105, 255),
                1.2f);
        }
    }

    // 3. Drill Motor / Head Unit (Cute golden-tan unit with grips)
    const ImVec2 unitP0(baseTip.x - 7.5f, baseTip.y - 20.0f);
    const ImVec2 unitP1(baseTip.x + 7.5f, baseTip.y - 11.0f);

    dl->AddRectFilled(
        ImVec2(unitP0.x, unitP0.y + 1.0f),
        ImVec2(unitP1.x, unitP1.y + 1.0f),
        IM_COL32(0, 0, 0, 50),
        4.0f);
    dl->AddRectFilled(unitP0, unitP1, IM_COL32(254, 240, 138, 255), 4.0f);
    dl->AddRect(unitP0, unitP1, IM_COL32(161, 98, 7, 255), 4.0f, 0, 1.2f);

    // Drill cute side handles
    dl->AddCircleFilled(ImVec2(unitP0.x - 1.5f, baseTip.y - 15.5f), 2.5f, IM_COL32(180, 83, 9, 255));
    dl->AddCircleFilled(ImVec2(unitP1.x + 1.5f, baseTip.y - 15.5f), 2.5f, IM_COL32(180, 83, 9, 255));

    // 4. Mascot Character (Blonde hair with animal ears holding the drill)
    const ImVec2 headCenter(baseTip.x, baseTip.y - 26.0f);

    // Animal ears (cat/bunny ears)
    dl->AddTriangleFilled(
        ImVec2(headCenter.x - 7.0f, headCenter.y - 4.0f),
        ImVec2(headCenter.x - 5.0f, headCenter.y - 12.0f),
        ImVec2(headCenter.x - 1.5f, headCenter.y - 6.0f),
        IM_COL32(146, 64, 14, 255));
    dl->AddTriangleFilled(
        ImVec2(headCenter.x + 1.5f, headCenter.y - 6.0f),
        ImVec2(headCenter.x + 5.0f, headCenter.y - 12.0f),
        ImVec2(headCenter.x + 7.0f, headCenter.y - 4.0f),
        IM_COL32(146, 64, 14, 255));
    dl->AddTriangleFilled(
        ImVec2(headCenter.x - 5.5f, headCenter.y - 5.0f),
        ImVec2(headCenter.x - 5.0f, headCenter.y - 10.0f),
        ImVec2(headCenter.x - 2.5f, headCenter.y - 6.0f),
        IM_COL32(244, 114, 182, 255));
    dl->AddTriangleFilled(
        ImVec2(headCenter.x + 2.5f, headCenter.y - 6.0f),
        ImVec2(headCenter.x + 5.0f, headCenter.y - 10.0f),
        ImVec2(headCenter.x + 5.5f, headCenter.y - 5.0f),
        IM_COL32(244, 114, 182, 255));

    // Blonde Hair Back & Face
    dl->AddCircleFilled(headCenter, 6.5f, IM_COL32(253, 224, 71, 255)); // Bright yellow hair
    dl->AddCircleFilled(ImVec2(headCenter.x, headCenter.y + 1.0f), 5.0f, IM_COL32(254, 226, 226, 255)); // Skin tone
    dl->AddCircle(headCenter, 6.5f, IM_COL32(180, 83, 9, 255), 12, 1.0f);

    // Front Hair Bangs
    dl->AddTriangleFilled(
        ImVec2(headCenter.x - 5.0f, headCenter.y - 3.0f),
        ImVec2(headCenter.x - 2.0f, headCenter.y - 1.0f),
        ImVec2(headCenter.x - 1.0f, headCenter.y - 5.0f),
        IM_COL32(253, 224, 71, 255));
    dl->AddTriangleFilled(
        ImVec2(headCenter.x + 1.0f, headCenter.y - 5.0f),
        ImVec2(headCenter.x + 2.0f, headCenter.y - 1.0f),
        ImVec2(headCenter.x + 5.0f, headCenter.y - 3.0f),
        IM_COL32(253, 224, 71, 255));

    // Cute Anime Eyes & Blush
    dl->AddCircleFilled(ImVec2(headCenter.x - 2.2f, headCenter.y + 0.5f), 1.0f, IM_COL32(69, 26, 3, 255));
    dl->AddCircleFilled(ImVec2(headCenter.x + 2.2f, headCenter.y + 0.5f), 1.0f, IM_COL32(69, 26, 3, 255));
    dl->AddCircleFilled(ImVec2(headCenter.x - 3.5f, headCenter.y + 2.2f), 1.2f, IM_COL32(244, 114, 182, 180)); // Left blush
    dl->AddCircleFilled(ImVec2(headCenter.x + 3.5f, headCenter.y + 2.2f), 1.2f, IM_COL32(244, 114, 182, 180)); // Right blush
    // Cute smile
    dl->AddLine(ImVec2(headCenter.x - 1.0f, headCenter.y + 3.0f), ImVec2(headCenter.x + 1.0f, headCenter.y + 3.0f), IM_COL32(180, 83, 9, 255), 1.0f);
}

void LauncherView::RenderTricksterProgressBar(
    float fraction,
    const ImVec2& size,
    bool showGears,
    bool showDrill,
    bool showRulerNumbers,
    ImFont* fontSmall) noexcept
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float t = static_cast<float>(ImGui::GetTime());

    // 1. Tactile shadow behind track
    dl->AddRectFilled(
        ImVec2(p0.x + 1.0f, p0.y + 1.5f),
        ImVec2(p1.x + 1.0f, p1.y + 1.5f),
        IM_COL32(0, 0, 0, 40),
        2.0f);

    // 2. Base Checkered Retro Gauge Background Track
    dl->AddRectFilled(p0, p1, IM_COL32(100, 116, 139, 200), 2.0f); // Slate-500

    // Draw subtle 3x3 checkered pixel grid
    dl->PushClipRect(p0, p1, true);
    const int checkSize = 3;
    const int numX = static_cast<int>(size.x) / checkSize + 1;
    const int numY = static_cast<int>(size.y) / checkSize + 1;
    for (int y = 0; y < numY; ++y)
    {
        for (int x = 0; x < numX; ++x)
        {
            if ((x + y) % 2 == 0)
            {
                dl->AddRectFilled(
                    ImVec2(p0.x + x * checkSize, p0.y + y * checkSize),
                    ImVec2(p0.x + (x + 1) * checkSize, p0.y + (y + 1) * checkSize),
                    IM_COL32(148, 163, 184, 120)); // Light tile
            }
        }
    }

    // 3. Ruler / Gauge Scale Numbers & Tick Marks (10, 20, 30, ... 100)
    if (showRulerNumbers && size.y >= 13.0f)
    {
        if (fontSmall) ImGui::PushFont(fontSmall);
        for (int pct = 10; pct <= 100; pct += 10)
        {
            const float markX = p0.x + size.x * (static_cast<float>(pct) / 100.0f);
            // Top and bottom tick markers
            dl->AddLine(
                ImVec2(markX, p0.y + 1.0f),
                ImVec2(markX, p0.y + 3.0f),
                IM_COL32(255, 255, 255, 150),
                1.0f);
            dl->AddLine(
                ImVec2(markX, p1.y - 3.0f),
                ImVec2(markX, p1.y - 1.0f),
                IM_COL32(255, 255, 255, 150),
                1.0f);

            // Scale number text (e.g. 70, 80, 90, 100)
            if (pct >= 50 || size.x > 300.0f)
            {
                char numStr[8]{};
                snprintf(numStr, sizeof(numStr), "%d", pct);
                ImVec2 numSize = ImGui::CalcTextSize(numStr);
                const float textX = markX - numSize.x * 0.5f;
                const float textY = p0.y + (size.y - numSize.y) * 0.5f;

                dl->AddText(ImVec2(textX + 0.5f, textY + 0.5f), IM_COL32(15, 23, 42, 160), numStr);
                dl->AddText(ImVec2(textX, textY), IM_COL32(241, 245, 249, 210), numStr);
            }
        }
        if (fontSmall) ImGui::PopFont();
    }

    dl->PopClipRect();

    // 4. Solid Bright Golden-Yellow Fill Bar
    const float f = (std::clamp)(fraction, 0.0f, 1.0f);
    const float pad = 1.0f;
    const ImVec2 barP0 = ImVec2(p0.x + pad, p0.y + pad);
    const float maxBarW = size.x - pad * 2.0f;
    const float barH = size.y - pad * 2.0f;
    const float barW = maxBarW * f;

    if (barW >= 1.0f)
    {
        const ImVec2 barP1 = ImVec2(barP0.x + barW, barP0.y + barH);

        // Gradient Golden Yellow Fill (#FFEB3B / #FFC107 / #F57F17)
        const ImU32 colTop = IM_COL32(255, 241, 118, 255);    // #fff176 Bright gold
        const ImU32 colMid = IM_COL32(255, 214, 0, 255);      // #ffd600 Rich Trickster yellow
        const ImU32 colBottom = IM_COL32(245, 158, 11, 255);  // #f59e0b Warm amber

        // Top half fill
        dl->AddRectFilledMultiColor(
            barP0,
            ImVec2(barP1.x, barP0.y + barH * 0.5f),
            colTop, colTop, colMid, colMid);

        // Bottom half fill
        dl->AddRectFilledMultiColor(
            ImVec2(barP0.x, barP0.y + barH * 0.5f),
            barP1,
            colMid, colMid, colBottom, colBottom);

        // Crisp Top Specular Sheen (White/Gold Highlight)
        dl->AddLine(
            ImVec2(barP0.x, barP0.y + 0.5f),
            ImVec2(barP1.x, barP0.y + 0.5f),
            IM_COL32(255, 255, 255, 230),
            1.0f);

        // Bottom Edge Bevel (Dark Amber)
        dl->AddLine(
            ImVec2(barP0.x, barP1.y - 0.5f),
            ImVec2(barP1.x, barP1.y - 0.5f),
            IM_COL32(217, 119, 6, 255),
            1.0f);

        // Animated diagonal sweep reflection
        dl->PushClipRect(barP0, barP1, true);
        const float skewX = barH * 0.7f;
        const float sweepPhase = fmodf(t * 1.15f, 2.0f) - 0.5f;
        const float sweepCenter = barP0.x + (maxBarW + skewX * 2.0f) * sweepPhase - skewX;

        dl->AddQuadFilled(
            ImVec2(sweepCenter + skewX - 16.0f, barP0.y),
            ImVec2(sweepCenter + skewX + 16.0f, barP0.y),
            ImVec2(sweepCenter - skewX + 16.0f, barP1.y),
            ImVec2(sweepCenter - skewX - 16.0f, barP1.y),
            IM_COL32(255, 255, 255, 75));

        dl->PopClipRect();

        // Right edge dividing border of the fill
        dl->AddLine(
            ImVec2(barP1.x - 0.5f, barP0.y),
            ImVec2(barP1.x - 0.5f, barP1.y),
            IM_COL32(217, 119, 6, 255),
            1.0f);
    }

    // 5. Outer Beveled White / Crisp Border
    dl->AddRect(
        p0,
        p1,
        IM_COL32(255, 255, 255, 255),
        2.0f,
        0,
        1.5f);
    dl->AddRect(
        ImVec2(p0.x - 1.0f, p0.y - 1.0f),
        ImVec2(p1.x + 1.0f, p1.y + 1.0f),
        IM_COL32(71, 85, 105, 170),
        2.5f,
        0,
        1.0f);

    // 6. Spinning Gears at Origin (Left side)
    if (showGears)
    {
        const bool isBusy = (f > 0.001f && f < 0.999f);
        const float gearSpeed = isBusy ? 2.5f : 0.6f;

        // Upper Orange Gear (overlapping top-left)
        RenderGear(
            dl,
            ImVec2(p0.x + 2.0f, p0.y + 1.0f),
            8.5f,
            7,
            IM_COL32(249, 115, 22, 255), // #f97316 Orange
            IM_COL32(194, 65, 12, 255),  // #c2410c Dark orange border
            IM_COL32(255, 237, 213, 255),// #ffedd5 Axle center
            t * gearSpeed);

        // Lower Lime Green Gear (overlapping bottom-left)
        RenderGear(
            dl,
            ImVec2(p0.x - 4.0f, p0.y + size.y - 1.0f),
            7.5f,
            6,
            IM_COL32(132, 204, 22, 255), // #84cc16 Lime
            IM_COL32(77, 124, 15, 255),  // #4d7c0f Dark lime border
            IM_COL32(236, 252, 203, 255),// #ecfccb Axle center
            -t * (gearSpeed * 1.2f));
    }

    // 7. Mascot Drill Indicator at the Leading Edge (barP1.x)
    if (showDrill && barW >= 2.0f)
    {
        const bool isDownloading = (f > 0.001f && f < 0.999f);
        const float jitterY = isDownloading ? (sinf(t * 35.0f) * 0.7f) : 0.0f;
        const float jitterX = isDownloading ? (cosf(t * 28.0f) * 0.35f) : 0.0f;
        const ImVec2 tipPos(barP0.x + barW + jitterX, p0.y + jitterY);

        // Sparkles floating around
        const float s1Alpha = sinf(t * 6.0f) * 0.35f + 0.65f;
        const float s2Alpha = cosf(t * 5.0f + 1.2f) * 0.35f + 0.65f;
        const float s3Alpha = sinf(t * 7.0f + 2.5f) * 0.35f + 0.65f;

        RenderSparkleStar(dl, ImVec2(tipPos.x - 14.0f, tipPos.y - 22.0f + sinf(t * 3.0f) * 2.0f), 4.5f, IM_COL32(56, 189, 248, 255), s1Alpha);
        RenderSparkleStar(dl, ImVec2(tipPos.x + 14.0f, tipPos.y - 24.0f + cosf(t * 3.5f) * 2.0f), 4.0f, IM_COL32(251, 191, 36, 255), s2Alpha);
        RenderSparkleStar(dl, ImVec2(tipPos.x - 16.0f, tipPos.y - 10.0f + sinf(t * 4.0f) * 1.5f), 3.5f, IM_COL32(244, 114, 182, 255), s3Alpha);
        RenderSparkleStar(dl, ImVec2(tipPos.x + 16.0f, tipPos.y - 12.0f + cosf(t * 4.5f) * 1.5f), 3.5f, IM_COL32(255, 255, 255, 255), s1Alpha);

        if (drillTexture_ && drillFrameCount_ > 0 && drillTexW_ > 0 && drillTexH_ > 0)
        {
            // Frame calculation: dynamically loop through drillFrameCount_ frames (~40 FPS downloading, ~20 FPS idle)
            const int frameIdx = static_cast<int>(fmodf(t * (isDownloading ? 40.0f : 20.0f), static_cast<float>(drillFrameCount_)));
            const float cellW = drillCellW_ > 0.0f ? drillCellW_ : 49.0f;
            const float cellH = drillCellH_ > 0.0f ? drillCellH_ : 64.0f;

            const float u0 = (frameIdx * cellW) / static_cast<float>(drillTexW_);
            const float v0 = 0.0f;
            const float u1 = ((frameIdx + 1) * cellW) / static_cast<float>(drillTexW_);
            const float v1 = cellH / static_cast<float>(drillTexH_);

            // Scaled render size (target ~44px height so all character classes look proportionate)
            const float scale = 44.0f / cellH;
            const float drawW = cellW * scale;
            const float drawH = cellH * scale;

            const ImVec2 spriteMin(tipPos.x - drawW * 0.5f, tipPos.y - drawH + 3.0f);
            const ImVec2 spriteMax(tipPos.x + drawW * 0.5f, tipPos.y + 3.0f);

            dl->AddImage(
                reinterpret_cast<ImTextureID>(drillTexture_),
                spriteMin,
                spriteMax,
                ImVec2(u0, v0),
                ImVec2(u1, v1),
                IM_COL32(255, 255, 255, 255));
        }
        else
        {
            RenderMascotDrillIndicator(dl, tipPos, t, isDownloading);
        }
    }

    ImGui::Dummy(size);
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
    // Redirect to the iconic Trickster Progress Bar
    RenderTricksterProgressBar(fraction, size, false, false, showPercentage, nullptr);
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

        // Logo Icon & Title
        const char* titleText = "Trickster Online";
        ImVec2 titleDim(0, 0);
        if (fontBold_) ImGui::PushFont(fontBold_);
        titleDim = ImGui::CalcTextSize(titleText);
        dl->AddText(ImVec2(16, 8), IM_COL32(15, 23, 42, 255), titleText);
        if (fontBold_) ImGui::PopFont();

        // Server Status Tag Pill
        const char* statusText = "Unknown";
        ImU32 bgCol = IM_COL32(241, 245, 249, 240);
        ImU32 borderCol = IM_COL32(203, 213, 225, 220);
        ImU32 textCol = IM_COL32(100, 116, 139, 255);
        ImU32 baseDotCol = IM_COL32(245, 158, 11, 255); // Amber for Unknown/Connecting

        if (state.serverStatus == ServerStatus::Online)
        {
            statusText = "Server Online";
            bgCol = IM_COL32(238, 242, 255, 240);
            borderCol = IM_COL32(165, 180, 252, 220);
            textCol = IM_COL32(29, 78, 216, 255);
            baseDotCol = IM_COL32(16, 185, 129, 255); // Green for Online
        }
        else if (state.serverStatus == ServerStatus::Maintenance)
        {
            statusText = "Maintenance";
            bgCol = IM_COL32(254, 226, 226, 240);
            borderCol = IM_COL32(248, 113, 113, 220);
            textCol = IM_COL32(220, 38, 38, 255);
            baseDotCol = IM_COL32(239, 68, 68, 255); // Red for Maintenance
        }

        const float pillX = 16.0f + titleDim.x + 12.0f;
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        const ImVec2 textDim = ImGui::CalcTextSize(statusText);
        const float pillWidth = textDim.x + 28.0f;
        const float pillY0 = 6.0f;
        const float pillY1 = 28.0f;
        const float pillMidY = (pillY0 + pillY1) * 0.5f;

        // Pill background & border
        dl->AddRectFilled(
            ImVec2(pillX, pillY0),
            ImVec2(pillX + pillWidth, pillY1),
            bgCol,
            11.0f);
        dl->AddRect(
            ImVec2(pillX, pillY0),
            ImVec2(pillX + pillWidth, pillY1),
            borderCol,
            11.0f,
            0,
            1.0f);

        // Glowing indicator circle
        const float t = static_cast<float>(ImGui::GetTime());
        const float pulseDot = (sinf(t * 4.0f) * 0.5f + 0.5f) * 55.0f + 200.0f;
        const int dotR = (baseDotCol >> IM_COL32_R_SHIFT) & 0xFF;
        const int dotG = (baseDotCol >> IM_COL32_G_SHIFT) & 0xFF;
        const int dotB = (baseDotCol >> IM_COL32_B_SHIFT) & 0xFF;
        const ImU32 dotCol = IM_COL32(dotR, dotG, dotB, static_cast<int>(pulseDot));
        dl->AddCircleFilled(ImVec2(pillX + 9.5f, pillMidY), 3.5f, dotCol);

        // Text vertically centered
        const float textY = pillY0 + (pillY1 - pillY0 - textDim.y) * 0.5f;
        dl->AddText(
            ImVec2(pillX + 18.0f, textY),
            textCol,
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

        // 3. Hero Banner Card Frame (17, 38, 523, 368)
        const ImVec2 heroMin(17.0f, 38.0f);
        const ImVec2 heroMax(523.0f, 368.0f);
        const float heroRounding = 12.0f;

        // Shadow behind Hero Card
        dl->AddRectFilled(
            ImVec2(heroMin.x, heroMin.y + 1.0f),
            ImVec2(heroMax.x, heroMax.y + 1.0f),
            IM_COL32(30, 70, 120, 20),
            heroRounding);

        if (heroTexture_)
        {
            dl->AddImageRounded(
                reinterpret_cast<ImTextureID>(heroTexture_),
                heroMin,
                heroMax,
                heroTexUvMin_,
                heroTexUvMax_,
                IM_COL32(255, 255, 255, 255),
                heroRounding);
        }
        else
        {
            // Stylized rich fallback Hero Banner background
            dl->AddRectFilledMultiColor(
                heroMin,
                heroMax,
                IM_COL32(15, 23, 42, 255),
                IM_COL32(30, 58, 138, 255),
                IM_COL32(88, 28, 135, 255),
                IM_COL32(17, 24, 39, 255));

            // Glowing hero accent circles & badge
            const float heroTime = static_cast<float>(ImGui::GetTime());
            const float pulseGlow = (sinf(heroTime * 2.0f) * 0.5f + 0.5f) * 40.0f + 60.0f;
            dl->AddCircleFilled(
                ImVec2((heroMin.x + heroMax.x) * 0.5f, (heroMin.y + heroMax.y) * 0.5f - 20.0f),
                90.0f,
                IM_COL32(59, 130, 246, static_cast<int>(pulseGlow)));

            if (fontLarge_) ImGui::PushFont(fontLarge_);
            dl->AddText(
                ImVec2(heroMin.x + 30.0f, heroMin.y + 120.0f),
                IM_COL32(255, 255, 255, 255),
                "TRICKSTER ONLINE");
            if (fontLarge_) ImGui::PopFont();

            if (fontRegular_) ImGui::PushFont(fontRegular_);
            dl->AddText(
                ImVec2(heroMin.x + 30.0f, heroMin.y + 160.0f),
                IM_COL32(226, 232, 240, 230),
                "The Awakening of the Crystals - Play Now!");
            if (fontRegular_) ImGui::PopFont();

            // Fallback Hero Card Border
            dl->AddRect(
                heroMin,
                heroMax,
                IM_COL32(186, 215, 243, 255),
                heroRounding,
                0,
                1.2f);
        }

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
            // Progress bar 1: File progress (Golden-Yellow Mini Gauge)
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 24));
            RenderTricksterProgressBar(
                animFileProgress_,
                ImVec2(348, 8),
                false,
                false,
                false,
                nullptr);

            // Progress bar 2: Total progress (Iconic Trickster Gauge with Gears, Ruler Scale & Drilling Mascot)
            ImGui::SetCursorPos(ImVec2(28, bottomCardY + 36));
            RenderTricksterProgressBar(
                animTotalProgress_,
                ImVec2(348, 18),
                true,
                true,
                true,
                fontSmall_);

            // Primary Action: Big Game Start Button (Transitions to Login Form on click)
            ImGui::SetCursorPos(ImVec2(winSize.x - 146, bottomCardY + 30));
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

    if (!showWindow)
        shouldClose_ = true;
}

static UINT GetNextPowerOfTwo(UINT n) noexcept
{
    UINT pow = 1;
    while (pow < n)
        pow <<= 1;
    return pow;
}

void LauncherView::LoadHeroTexture(IDirect3DDevice9* device, const std::wstring& exeDir) noexcept
{
    if (!device) return;
    if (heroTexture_) return;

    if (!exeDir.empty())
        heroExeDir_ = exeDir;

    std::vector<std::filesystem::path> candidatePaths;
    if (!heroExeDir_.empty())
    {
        const std::filesystem::path dir(heroExeDir_);
        const std::filesystem::path heroDir = dir / L"LauncherData" / L"assets" / L"hero";
        candidatePaths.push_back(heroDir / L"hero.png");
        candidatePaths.push_back(heroDir / L"hero.jpg");
        candidatePaths.push_back(heroDir / L"hero.jpeg");
        candidatePaths.push_back(heroDir / L"hero_banner.png");
        candidatePaths.push_back(heroDir / L"hero_banner.jpg");
        candidatePaths.push_back(dir / L"hero.png");
        candidatePaths.push_back(dir / L"hero.jpg");
        candidatePaths.push_back(dir / L"hero.jpeg");
        candidatePaths.push_back(dir / L"hero_banner.png");
        candidatePaths.push_back(dir / L"hero_banner.jpg");
    }
    candidatePaths.push_back(L"LauncherData/assets/hero/hero.png");
    candidatePaths.push_back(L"LauncherData/assets/hero/hero.jpg");
    candidatePaths.push_back(L"LauncherData/assets/hero/hero.jpeg");
    candidatePaths.push_back(L"LauncherData/assets/hero/hero_banner.png");
    candidatePaths.push_back(L"LauncherData/assets/hero/hero_banner.jpg");
    candidatePaths.push_back(L"hero.png");
    candidatePaths.push_back(L"hero.jpg");
    candidatePaths.push_back(L"hero.jpeg");
    candidatePaths.push_back(L"hero_banner.png");
    candidatePaths.push_back(L"hero_banner.jpg");

    int width = 0, height = 0, channels = 0;
    unsigned char* data = nullptr;
    std::string loadedPath;

    for (const auto& p : candidatePaths)
    {
        if (!std::filesystem::exists(p))
            continue;

        FILE* f = nullptr;
        if (_wfopen_s(&f, p.c_str(), L"rb") == 0 && f)
        {
            data = stbi_load_from_file(f, &width, &height, &channels, 4);
            fclose(f);
            if (data)
            {
                loadedPath = p.string();
                break;
            }
        }
    }

    if (!data)
    {
        const char* err = stbi_failure_reason();
        Logger::LogError("Hero texture load failed: Could not load image from candidate paths. STB reason: " + std::string(err ? err : "none"));
        return;
    }

    const UINT texW = GetNextPowerOfTwo(static_cast<UINT>(width));
    const UINT texH = GetNextPowerOfTwo(static_cast<UINT>(height));

    LPDIRECT3DTEXTURE9 texture = nullptr;

    // Direct3D 9Ex requires D3DPOOL_DEFAULT with D3DUSAGE_DYNAMIC (D3DPOOL_MANAGED is invalid in D3D9Ex)
    HRESULT hr = device->CreateTexture(
        texW,
        texH,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &texture,
        nullptr);

    if (FAILED(hr) || !texture)
    {
        // Fallback for classic Direct3D 9 (non-Ex)
        hr = device->CreateTexture(
            texW,
            texH,
            1,
            0,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            &texture,
            nullptr);
    }

    if (FAILED(hr) || !texture)
    {
        Logger::LogError("Hero texture CreateTexture failed hr=" + std::to_string(hr) + " for padded dimensions " + std::to_string(texW) + "x" + std::to_string(texH));
        stbi_image_free(data);
        return;
    }

    D3DLOCKED_RECT rect;
    // Try lock with D3DLOCK_DISCARD (for D3DUSAGE_DYNAMIC) or 0
    HRESULT lockHr = texture->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD);
    if (FAILED(lockHr))
        lockHr = texture->LockRect(0, &rect, nullptr, 0);

    if (SUCCEEDED(lockHr))
    {
        unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
        std::memset(dest, 0, rect.Pitch * texH);

        for (int y = 0; y < height; ++y)
        {
            unsigned char* rowDest = dest + y * rect.Pitch;
            const unsigned char* rowSrc = data + y * width * 4;
            for (int x = 0; x < width; ++x)
            {
                rowDest[x * 4 + 0] = rowSrc[x * 4 + 2]; // B
                rowDest[x * 4 + 1] = rowSrc[x * 4 + 1]; // G
                rowDest[x * 4 + 2] = rowSrc[x * 4 + 0]; // R
                rowDest[x * 4 + 3] = rowSrc[x * 4 + 3]; // A
            }
        }
        texture->UnlockRect(0);
        heroTexture_ = texture;

        // Container dimensions: (523 - 17) x (368 - 38) = 506 x 330
        constexpr float targetW = 506.0f;
        constexpr float targetH = 330.0f;
        constexpr float targetAspect = targetW / targetH;

        const float imgW = static_cast<float>(width);
        const float imgH = static_cast<float>(height);
        const float imgAspect = imgW / imgH;

        float cropX0 = 0.0f;
        float cropY0 = 0.0f;
        float cropX1 = imgW;
        float cropY1 = imgH;

        if (imgAspect > targetAspect)
        {
            // Wider than container: center crop left and right
            const float visibleW = imgH * targetAspect;
            const float offset = (imgW - visibleW) * 0.5f;
            cropX0 = offset;
            cropX1 = offset + visibleW;
        }
        else if (imgAspect < targetAspect)
        {
            // Taller than container: center crop top and bottom
            const float visibleH = imgW / targetAspect;
            const float offset = (imgH - visibleH) * 0.5f;
            cropY0 = offset;
            cropY1 = offset + visibleH;
        }

        heroTexUvMin_ = ImVec2(
            cropX0 / static_cast<float>(texW),
            cropY0 / static_cast<float>(texH));
        heroTexUvMax_ = ImVec2(
            cropX1 / static_cast<float>(texW),
            cropY1 / static_cast<float>(texH));

        Logger::LogError("Hero texture loaded successfully from '" + loadedPath + "' (" + std::to_string(width) + "x" + std::to_string(height) + " padded to " + std::to_string(texW) + "x" + std::to_string(texH) + ")");
    }
    else
    {
        Logger::LogError("Hero texture LockRect failed hr=" + std::to_string(lockHr));
        texture->Release();
    }

    stbi_image_free(data);
}

void LauncherView::UnloadHeroTexture() noexcept
{
    if (heroTexture_)
    {
        heroTexture_->Release();
        heroTexture_ = nullptr;
    }
}

void LauncherView::LoadDrillTexture(IDirect3DDevice9* device, const std::wstring& exeDir) noexcept
{
    if (!device) return;
    if (drillTexture_) return;

    // Find valid drill root directories
    std::vector<std::filesystem::path> rootCandidates;
    if (!exeDir.empty())
    {
        const std::filesystem::path dir(exeDir);
        rootCandidates.push_back(dir / L"LauncherData" / L"assets" / L"drill");
        rootCandidates.push_back(dir / L"assets" / L"drill");
    }
    rootCandidates.push_back(L"LauncherData/assets/drill");
    rootCandidates.push_back(L"assets/drill");

    std::filesystem::path drillRoot;
    for (const auto& rc : rootCandidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(rc, ec) && std::filesystem::is_directory(rc, ec))
        {
            drillRoot = rc;
            break;
        }
    }

    if (drillRoot.empty())
        return;

    // Determine which character folder to use
    std::filesystem::path targetFolder;
    if (!selectedDrillDir_.empty())
    {
        std::error_code ec;
        if (std::filesystem::exists(selectedDrillDir_, ec) && std::filesystem::is_directory(selectedDrillDir_, ec))
        {
            targetFolder = selectedDrillDir_;
        }
    }

    if (targetFolder.empty())
    {
        // Enumerate all subdirectories in drillRoot
        std::vector<std::filesystem::path> subDirs;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(drillRoot, ec))
        {
            if (entry.is_directory(ec))
            {
                subDirs.push_back(entry.path());
            }
        }

        if (!subDirs.empty())
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0, subDirs.size() - 1);
            targetFolder = subDirs[dist(gen)];
            selectedDrillDir_ = targetFolder.wstring();
            Logger::Log("Random drill character chosen: " + targetFolder.filename().string());
        }
        else
        {
            targetFolder = drillRoot;
            selectedDrillDir_ = targetFolder.wstring();
        }
    }

    // Now load animation frames from targetFolder
    struct RawFrame
    {
        int w = 0;
        int h = 0;
        unsigned char* data = nullptr;
    };
    std::vector<RawFrame> loadedFrames;

    // Check if targetFolder has a single spritesheet
    const std::filesystem::path sheetPath = targetFolder / L"drill_sheet.png";
    std::error_code ec;
    if (std::filesystem::exists(sheetPath, ec) && !std::filesystem::is_directory(sheetPath, ec))
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, sheetPath.c_str(), L"rb") == 0 && f)
        {
            int w = 0, h = 0, c = 0;
            unsigned char* px = stbi_load_from_file(f, &w, &h, &c, 4);
            fclose(f);
            if (px)
            {
                loadedFrames.push_back({ w, h, px });
            }
        }
    }

    // If no sheet, search for individual frame PNGs / BMPs
    if (loadedFrames.empty())
    {
        std::vector<std::filesystem::path> frameFiles;
        for (const auto& entry : std::filesystem::directory_iterator(targetFolder, ec))
        {
            if (!entry.is_regular_file(ec))
                continue;
            const std::string ext = entry.path().extension().string();
            if (_stricmp(ext.c_str(), ".png") == 0 || _stricmp(ext.c_str(), ".bmp") == 0)
            {
                if (entry.path().filename().string() != "drill_sheet.png")
                {
                    frameFiles.push_back(entry.path());
                }
            }
        }

        auto extractNumber = [](const std::filesystem::path& p) -> int
        {
            const std::string stem = p.stem().string();
            std::string num;
            for (char ch : stem)
            {
                if (std::isdigit(static_cast<unsigned char>(ch)))
                    num += ch;
            }
            return num.empty() ? 0 : std::atoi(num.c_str());
        };

        std::sort(frameFiles.begin(), frameFiles.end(), [&](const auto& a, const auto& b)
        {
            int numA = extractNumber(a);
            int numB = extractNumber(b);
            if (numA != numB) return numA < numB;
            return a.filename().string() < b.filename().string();
        });

        for (const auto& fp : frameFiles)
        {
            FILE* f = nullptr;
            if (_wfopen_s(&f, fp.c_str(), L"rb") == 0 && f)
            {
                int w = 0, h = 0, c = 0;
                unsigned char* px = stbi_load_from_file(f, &w, &h, &c, 4);
                fclose(f);
                if (px)
                {
                    loadedFrames.push_back({ w, h, px });
                }
            }
        }
    }

    if (loadedFrames.empty())
        return;

    // Calculate maximum frame bounds
    int maxW = 0, maxH = 0;
    for (const auto& fr : loadedFrames)
    {
        maxW = std::max(maxW, fr.w);
        maxH = std::max(maxH, fr.h);
    }

    if (maxW <= 0 || maxH <= 0)
    {
        for (auto& fr : loadedFrames)
            stbi_image_free(fr.data);
        return;
    }

    const int frameCount = static_cast<int>(loadedFrames.size());
    const int totalW = maxW * frameCount;
    const int totalH = maxH;

    // Stitch all frames side-by-side into a continuous buffer, bottom-centered
    std::vector<unsigned char> stitched(static_cast<size_t>(totalW * totalH * 4), 0);
    for (int i = 0; i < frameCount; ++i)
    {
        const auto& fr = loadedFrames[i];
        const int offX = i * maxW + (maxW - fr.w) / 2;
        const int offY = maxH - fr.h; // Bottom align so drilling tip and base stay ground-aligned

        for (int y = 0; y < fr.h; ++y)
        {
            unsigned char* dst = stitched.data() + ((offY + y) * totalW + offX) * 4;
            const unsigned char* src = fr.data + (y * fr.w) * 4;
            std::memcpy(dst, src, static_cast<size_t>(fr.w * 4));
        }
        stbi_image_free(fr.data);
    }

    const UINT texW = GetNextPowerOfTwo(static_cast<UINT>(totalW));
    const UINT texH = GetNextPowerOfTwo(static_cast<UINT>(totalH));

    LPDIRECT3DTEXTURE9 texture = nullptr;
    HRESULT hr = device->CreateTexture(
        texW,
        texH,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &texture,
        nullptr);

    if (FAILED(hr) || !texture)
    {
        hr = device->CreateTexture(
            texW,
            texH,
            1,
            0,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            &texture,
            nullptr);
    }

    if (FAILED(hr) || !texture)
        return;

    D3DLOCKED_RECT rect;
    HRESULT lockHr = texture->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD);
    if (FAILED(lockHr))
        lockHr = texture->LockRect(0, &rect, nullptr, 0);

    if (SUCCEEDED(lockHr))
    {
        unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
        std::memset(dest, 0, rect.Pitch * texH);

        for (int y = 0; y < totalH; ++y)
        {
            unsigned char* rowDest = dest + y * rect.Pitch;
            const unsigned char* rowSrc = stitched.data() + y * totalW * 4;
            for (int x = 0; x < totalW; ++x)
            {
                rowDest[x * 4 + 0] = rowSrc[x * 4 + 2]; // B
                rowDest[x * 4 + 1] = rowSrc[x * 4 + 1]; // G
                rowDest[x * 4 + 2] = rowSrc[x * 4 + 0]; // R
                rowDest[x * 4 + 3] = rowSrc[x * 4 + 3]; // A
            }
        }
        texture->UnlockRect(0);
        drillTexture_ = texture;
        drillTexW_ = texW;
        drillTexH_ = texH;
        drillCellW_ = static_cast<float>(maxW);
        drillCellH_ = static_cast<float>(maxH);
        drillFrameCount_ = frameCount;
    }
    else
    {
        texture->Release();
    }
}

void LauncherView::UnloadDrillTexture() noexcept
{
    if (drillTexture_)
    {
        drillTexture_->Release();
        drillTexture_ = nullptr;
        drillTexW_ = 0;
        drillTexH_ = 0;
    }
}
