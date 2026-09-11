#include "Gui.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <d3dx9.h>
#include <shellapi.h>

#include "Config.h"
#include "Helper.h"
#include "Language.h"
#include "LauncherState.h"
#include "RendererD3D9.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "resource.h"

namespace
{
struct TexturePair
{
    IDirect3DTexture9* color = nullptr;
    IDirect3DTexture9* gray = nullptr;
};

HWND s_window = nullptr;
RendererD3D9 s_renderer;
bool s_isRunning = true;
bool s_isVerifying = false;
bool s_showMainWindow = true;

ImFont* s_fontSmall = nullptr;
ImFont* s_fontRegular = nullptr;
Helper* s_helper = nullptr;

LPDIRECT3DTEXTURE9 s_bgTex = nullptr;
LPDIRECT3DTEXTURE9 s_logoTex = nullptr;
LPDIRECT3DTEXTURE9 s_option_n = nullptr;
LPDIRECT3DTEXTURE9 s_option_h = nullptr;
LPDIRECT3DTEXTURE9 s_option_s = nullptr;
LPDIRECT3DTEXTURE9 s_option_g = nullptr;
LPDIRECT3DTEXTURE9 s_exit_n = nullptr;
LPDIRECT3DTEXTURE9 s_exit_h = nullptr;
LPDIRECT3DTEXTURE9 s_exit_s = nullptr;
LPDIRECT3DTEXTURE9 s_game_n = nullptr;
LPDIRECT3DTEXTURE9 s_game_h = nullptr;
LPDIRECT3DTEXTURE9 s_game_s = nullptr;
LPDIRECT3DTEXTURE9 s_game_g = nullptr;
LPDIRECT3DTEXTURE9 s_check_n = nullptr;
LPDIRECT3DTEXTURE9 s_check_h = nullptr;
LPDIRECT3DTEXTURE9 s_check_s = nullptr;
LPDIRECT3DTEXTURE9 s_check_g = nullptr;

bool s_check_l = true;
bool s_option_l = true;
bool s_exit_l = false;
bool s_game_l = true;
bool s_check_f = false;

IDirect3DTexture9* LoadTextureFromResource(HINSTANCE hInstance, int resourceId, int desiredWidth, int desiredHeight) noexcept
{
    if (!s_renderer.Device())
        return nullptr;

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), L"PNG");
    if (!hRes)
        return nullptr;

    DWORD resSize = SizeofResource(hInstance, hRes);
    HGLOBAL hMem = LoadResource(hInstance, hRes);
    void* pData = LockResource(hMem);
    if (!pData)
        return nullptr;

    IDirect3DTexture9* texture = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
        s_renderer.Device(),
        pData,
        resSize,
        desiredWidth,
        desiredHeight,
        D3DX_DEFAULT,
        0,
        D3DFMT_UNKNOWN,
        D3DPOOL_MANAGED,
        D3DX_FILTER_NONE,
        D3DX_FILTER_NONE,
        0,
        nullptr,
        nullptr,
        &texture);

    return SUCCEEDED(hr) ? texture : nullptr;
}

TexturePair LoadTexturePairFromResource(HINSTANCE hInstance, int resourceId, int desiredWidth, int desiredHeight) noexcept
{
    TexturePair result = { nullptr, nullptr };
    if (!s_renderer.Device())
        return result;

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), L"PNG");
    if (!hRes)
        return result;

    DWORD resSize = SizeofResource(hInstance, hRes);
    HGLOBAL hMem = LoadResource(hInstance, hRes);
    void* pData = LockResource(hMem);
    if (!pData)
        return result;

    IDirect3DTexture9* texColor = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
        s_renderer.Device(),
        pData,
        resSize,
        desiredWidth,
        desiredHeight,
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
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
    hr = s_renderer.Device()->CreateTexture(
        desiredWidth,
        desiredHeight,
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
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

bool TricksterImageButton(
    const char* id,
    ImTextureID normal,
    ImTextureID hover,
    ImTextureID pressed,
    ImTextureID locked,
    const ImVec2& size,
    bool& IsLocked) noexcept
{
    ImGui::PushID(id);
    bool pressed_result = ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImTextureID tex = normal;

    if (!IsLocked)
    {
        if (held)
            tex = pressed;
        else if (hovered)
            tex = hover;
    }
    else
    {
        tex = locked;
    }

    dl->AddImage(tex, min, max);
    ImGui::PopID();
    return pressed_result;
}

void SetupImGuiStyle() noexcept
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
}

void InitFonts(float baseFontSize = 16.0f) noexcept
{
    char windowsDir[MAX_PATH];
    GetWindowsDirectoryA(windowsDir, MAX_PATH);
    std::string fontPath = std::string(windowsDir) + "\\Fonts\\segoeuib.ttf";

    const char* regularFontPath = fontPath.c_str();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    float smallFontSize = baseFontSize * 0.8f;

    auto LoadFont = [&](const char* path, float size) -> ImFont*
    {
        ImFontConfig fontCfg;
        fontCfg.MergeMode = false;
        ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &fontCfg, glyphRanges);
        return font;
    };

    s_fontSmall = LoadFont(regularFontPath, smallFontSize);
    s_fontRegular = LoadFont(regularFontPath, baseFontSize);
    io.FontDefault = s_fontRegular ? s_fontRegular : io.Fonts->Fonts[0];
    io.Fonts->Build();
}

void SafeReleaseTexture(LPDIRECT3DTEXTURE9& tex) noexcept
{
    if (tex)
    {
        tex->Release();
        tex = nullptr;
    }
}
} // namespace

namespace gui
{
void SetWindow(HWND window) noexcept
{
    s_window = window;
}

HWND GetWindow() noexcept
{
    return s_window;
}

bool CreateDevice() noexcept
{
    return s_renderer.Create(s_window);
}

void ResetDevice() noexcept
{
    s_renderer.Reset();
}

void DestroyDevice() noexcept
{
    s_renderer.Destroy();
}

LPDIRECT3DDEVICE9 GetDevice() noexcept
{
    return s_renderer.Device();
}

void HandleResize(UINT width, UINT height) noexcept
{
    if (s_renderer.Device())
    {
        s_renderer.SetBackBufferSize(width, height);
        s_renderer.Reset();
    }
}

bool IsRunning() noexcept
{
    return s_isRunning;
}

void SetRunning(bool running) noexcept
{
    s_isRunning = running;
}

void OpenURL(const char* url) noexcept
{
    if (url && *url)
        ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void RenderLink(const char* label, const char* url) noexcept
{
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 209, 97, 255));
    if (ImGui::Text("%s", label); ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked())
            OpenURL(url);
    }
    ImGui::PopStyleColor();
}

void LoadResources(HINSTANCE hInstance) noexcept
{
    if (!hInstance)
        hInstance = GetModuleHandleW(nullptr);

    s_bgTex = LoadTextureFromResource(hInstance, IDB_BG, WIDTH, HEIGHT);
    s_logoTex = LoadTextureFromResource(hInstance, IDB_LOGO, 185, 71);

    TexturePair optionTex = LoadTexturePairFromResource(hInstance, OPTION_N, 113, 27);
    s_option_n = optionTex.color;
    s_option_g = optionTex.gray;
    s_option_h = LoadTextureFromResource(hInstance, OPTION_H, 113, 27);
    s_option_s = LoadTextureFromResource(hInstance, OPTION_S, 113, 27);

    s_exit_n = LoadTextureFromResource(hInstance, EXIT_N, 113, 27);
    s_exit_h = LoadTextureFromResource(hInstance, EXIT_H, 113, 27);
    s_exit_s = LoadTextureFromResource(hInstance, EXIT_S, 113, 27);

    TexturePair gameTex = LoadTexturePairFromResource(hInstance, GAME_N, 118, 61);
    s_game_n = gameTex.color;
    s_game_g = gameTex.gray;
    s_game_h = LoadTextureFromResource(hInstance, GAME_H, 118, 61);
    s_game_s = LoadTextureFromResource(hInstance, GAME_S, 118, 61);

    TexturePair checkTex = LoadTexturePairFromResource(hInstance, CHECK_N, 113, 27);
    s_check_n = checkTex.color;
    s_check_g = checkTex.gray;
    s_check_h = LoadTextureFromResource(hInstance, CHECK_H, 113, 27);
    s_check_s = LoadTextureFromResource(hInstance, CHECK_S, 113, 27);
}

void CreateImGui() noexcept
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    if (!s_helper)
    {
        s_helper = new Helper();
        std::string maintenanceCheck = s_helper->GetFileFromURL(0);
        if (maintenanceCheck.empty())
        {
            MessageBoxA(nullptr, "Error!", nullptr, MB_OK);
            PostQuitMessage(0);
        }
        else
        {
            s_helper->isMaintenance = (maintenanceCheck == "true");
        }
    }

    InitFonts();
    ImGuiIO& io = ImGui::GetIO();
    SetupImGuiStyle();
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(s_window);
    ImGui_ImplDX9_Init(s_renderer.Device());
}

void DestroyImGui() noexcept
{
    SafeReleaseTexture(s_bgTex);
    SafeReleaseTexture(s_logoTex);
    SafeReleaseTexture(s_option_n);
    SafeReleaseTexture(s_option_h);
    SafeReleaseTexture(s_option_s);
    SafeReleaseTexture(s_option_g);
    SafeReleaseTexture(s_exit_n);
    SafeReleaseTexture(s_exit_h);
    SafeReleaseTexture(s_exit_s);
    SafeReleaseTexture(s_game_n);
    SafeReleaseTexture(s_game_h);
    SafeReleaseTexture(s_game_s);
    SafeReleaseTexture(s_game_g);
    SafeReleaseTexture(s_check_n);
    SafeReleaseTexture(s_check_h);
    SafeReleaseTexture(s_check_s);
    SafeReleaseTexture(s_check_g);

    delete s_helper;
    s_helper = nullptr;

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_renderer.Destroy();
}

void BeginRender() noexcept
{
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EndRender() noexcept
{
    ImGui::EndFrame();
    if (s_renderer.BeginFrame())
    {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    s_renderer.EndFrame();
}

void Render() noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ static_cast<float>(WIDTH), static_cast<float>(HEIGHT) });
    bool opened = ImGui::Begin(
        " ",
        &s_showMainWindow,
        nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (opened)
    {
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(s_bgTex), ImVec2(0, 0), io.DisplaySize);
        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec2 subTitleSize = ImGui::CalcTextSize(config::SubTitle.c_str());
        ImGui::SetCursorPos(ImVec2(winSize.x - subTitleSize.x - 15, 8));
        ImGui::Text("%s", config::SubTitle.c_str());

        ImGui::SetCursorPos(ImVec2(11, winSize.y - 98));
        ImGui::Image(reinterpret_cast<ImTextureID>(s_logoTex), ImVec2(185, 71));

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        float fileProgress = LauncherState::fileProgress.load(std::memory_order_relaxed);
        float totalProgress = LauncherState::totalProgress.load(std::memory_order_relaxed);
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 165));
        ImGui::ProgressBar(fileProgress, ImVec2(362, 7), " ");
        ImGui::PopStyleColor(2);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(133 / 255.f, 242 / 255.f, 254 / 255.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(133 / 255.f, 242 / 255.f, 254 / 255.f, 1.0f));
        ImGui::SetCursorPos(ImVec2(24, winSize.y - 152));
        ImGui::ProgressBar(totalProgress, ImVec2(362, 12), " ");
        ImGui::PopStyleColor(2);

        ImGui::SetCursorPos(ImVec2(23, winSize.y - 185));
        static std::string cachedFileString;
        static std::string cachedSpeedString;
        static auto lastStringUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastStringUpdate > std::chrono::milliseconds(100))
        {
            {
                std::lock_guard<std::mutex> lockFile(LauncherState::fileStringMutex);
                cachedFileString = LauncherState::fileString;
            }
            {
                std::lock_guard<std::mutex> lockSpeed(LauncherState::speedStringMutex);
                cachedSpeedString = LauncherState::speedString;
            }
            lastStringUpdate = now;
        }

        ImGui::Text("%s", cachedFileString.c_str());
        ImVec2 textSize = ImGui::CalcTextSize(cachedSpeedString.c_str());
        ImGui::SetCursorPos(ImVec2(winSize.x - textSize.x - 24, winSize.y - 185));
        ImGui::Text("%s", cachedSpeedString.c_str());

        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 76));
        ImGui::Text("%s", lang::GetString("launcher_site_desc").c_str());

        ImGui::SetCursorPos(ImVec2(winSize.x - 328, winSize.y - 60));
        RenderLink(lang::GetString("launcher_site_click").c_str(), config::WebsiteLink.c_str());

        ImGui::SetCursorPos(ImVec2(23, winSize.y - 132));
        if (TricksterImageButton(
                lang::GetString("launcher_check").c_str(),
                reinterpret_cast<ImTextureID>(s_check_n),
                reinterpret_cast<ImTextureID>(s_check_h),
                reinterpret_cast<ImTextureID>(s_check_s),
                reinterpret_cast<ImTextureID>(s_check_g),
                ImVec2(113, 27),
                s_check_l))
        {
            if (!s_check_l && s_helper)
            {
                s_isVerifying = false;
                s_check_f = true;
                s_helper->isWorkerDone = false;
            }
        }

        ImGui::SetCursorPos(ImVec2(150, winSize.y - 132));
        if (TricksterImageButton(
                lang::GetString("launcher_options").c_str(),
                reinterpret_cast<ImTextureID>(s_option_n),
                reinterpret_cast<ImTextureID>(s_option_h),
                reinterpret_cast<ImTextureID>(s_option_s),
                reinterpret_cast<ImTextureID>(s_option_g),
                ImVec2(113, 27),
                s_option_l))
        {
            if (!s_option_l && s_helper)
            {
                STARTUPINFOA si = { sizeof(si) };
                PROCESS_INFORMATION pi{};
                std::filesystem::path gameExe = s_helper->GetGamePath() / config::OptionExecName.c_str();
                std::string exePath = gameExe.string();
                if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
                    MessageBoxA(nullptr, lang::GetString("launcher_setup_fail").c_str(), "Error!", MB_OK);
            }
        }

        ImGui::SetCursorPos(ImVec2(274, winSize.y - 132));
        if (TricksterImageButton(
                lang::GetString("launcher_exit").c_str(),
                reinterpret_cast<ImTextureID>(s_exit_n),
                reinterpret_cast<ImTextureID>(s_exit_h),
                reinterpret_cast<ImTextureID>(s_exit_s),
                reinterpret_cast<ImTextureID>(s_exit_s),
                ImVec2(113, 27),
                s_exit_l))
        {
            s_isRunning = false;
        }

        ImGui::SetCursorPos(ImVec2(winSize.x - 139, winSize.y - 166));
        if (TricksterImageButton(
                lang::GetString("launcher_game_start").c_str(),
                reinterpret_cast<ImTextureID>(s_game_n),
                reinterpret_cast<ImTextureID>(s_game_h),
                reinterpret_cast<ImTextureID>(s_game_s),
                reinterpret_cast<ImTextureID>(s_game_g),
                ImVec2(118, 61),
                s_game_l))
        {
            if (!s_game_l && s_helper)
                s_helper->ClickPlayButton();
        }

        if (s_helper)
        {
            if (s_helper->isWorkerDone)
            {
                s_check_l = false;
                s_option_l = false;
                if (!s_helper->isMaintenance)
                    s_game_l = false;
            }
            else
            {
                s_check_l = true;
                s_option_l = true;
                s_game_l = true;
            }

            if (!s_isVerifying)
            {
                s_helper->UpdateLauncher();
                s_helper->CheckWorker(s_check_f);
                s_isVerifying = true;
            }
        }

        ImGui::End();
    }

    if (!s_showMainWindow)
        s_isRunning = false;
}
} // namespace gui
