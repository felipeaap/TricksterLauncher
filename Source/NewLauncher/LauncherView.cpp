#include "LauncherView.h"

#include <algorithm>
#define NOMINMAX
#include <windows.h>

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

void LauncherView::Render(
    const LauncherViewState& state,
    const TextureManager& textures,
    const LauncherViewEvents& events) noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ static_cast<float>(kWidth), static_cast<float>(kHeight) });

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

        // Progress bar 1: File progress
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 165));
        ImGui::ProgressBar(state.fileProgress, ImVec2(362, 7), " ");
        ImGui::PopStyleColor(2);

        // Progress bar 2: Total progress
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(133 / 255.f, 242 / 255.f, 254 / 255.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(133 / 255.f, 242 / 255.f, 254 / 255.f, 1.0f));
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 152));
        ImGui::ProgressBar(state.totalProgress, ImVec2(362, 12), " ");
        ImGui::PopStyleColor(2);

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
