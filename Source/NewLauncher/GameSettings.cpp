#include "GameSettings.h"
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include "Logger.h"

namespace
{
    const std::vector<ResolutionOption> s_resolutions = {
        { 800,  600,  "800 x 600 (4:3)" },
        { 1024, 768,  "1024 x 768 (4:3)" },
        { 1280, 720,  "1280 x 720 (16:9 HD)" },
        { 1280, 800,  "1280 x 800 (16:10)" },
        { 1366, 768,  "1366 x 768 (16:9)" },
        { 1440, 900,  "1440 x 900 (16:10)" },
        { 1600, 900,  "1600 x 900 (16:9)" },
        { 1680, 1050, "1680 x 1050 (16:10)" },
        { 1920, 1080, "1920 x 1080 (16:9 Full HD)" }
    };

    DWORD ReadDword(HKEY hKey, const wchar_t* valueName, DWORD defaultValue) noexcept
    {
        DWORD type = 0;
        DWORD data = 0;
        DWORD size = sizeof(data);
        LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size);
        if (status == ERROR_SUCCESS && type == REG_DWORD)
        {
            return data;
        }
        return defaultValue;
    }

    std::string ReadString(HKEY hKey, const wchar_t* valueName, const std::string& defaultValue) noexcept
    {
        wchar_t buffer[256]{};
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size);
        if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0)
            {
                std::string result(utf8Len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &result[0], utf8Len, nullptr, nullptr);
                return result;
            }
        }
        return defaultValue;
    }

    bool WriteDword(HKEY hKey, const wchar_t* valueName, DWORD value) noexcept
    {
        LONG status = RegSetValueExW(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        return (status == ERROR_SUCCESS);
    }

    bool WriteString(HKEY hKey, const wchar_t* valueName, const std::string& value) noexcept
    {
        const int wideLen = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (wideLen <= 0)
            return false;

        std::vector<wchar_t> wideBuf(wideLen);
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wideBuf.data(), wideLen);

        LONG status = RegSetValueExW(
            hKey,
            valueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(wideBuf.data()),
            static_cast<DWORD>(wideBuf.size() * sizeof(wchar_t)));

        return (status == ERROR_SUCCESS);
    }
}

const std::vector<ResolutionOption>& GameSettings::GetSupportedResolutions() noexcept
{
    return s_resolutions;
}

GameConfig GameSettings::GetDefaults() noexcept
{
    GameConfig cfg;
    cfg.widthPixel = 1024;
    cfg.heightPixel = 768;
    cfg.fullScreen = false;
    cfg.useSound = true;
    cfg.soundFrequency = 44100;
    cfg.soundBit = 16;
    cfg.soundStereo = true;
    cfg.sample2D = 32;
    cfg.sample3D = 32;
    cfg.streamSample = 32;
    cfg.use3dEffect = true;
    cfg.useMapEffect = true;
    cfg.captureFormat = "JPG";
    cfg.captureQuality = "Very High";
    return cfg;
}

GameConfig GameSettings::Load() noexcept
{
    GameConfig cfg = GetDefaults();
    HKEY hKey = nullptr;

    LONG status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        GetRegistrySubKey(),
        0,
        KEY_READ,
        &hKey);

    if (status != ERROR_SUCCESS)
    {
        Logger::Log("GameSettings: Key HKCU\\Software\\Trickster_NT does not exist yet; using default settings.");
        return cfg;
    }

    cfg.widthPixel = static_cast<int>(ReadDword(hKey, L"widthPixel", static_cast<DWORD>(cfg.widthPixel)));
    cfg.heightPixel = static_cast<int>(ReadDword(hKey, L"heightPixel", static_cast<DWORD>(cfg.heightPixel)));
    cfg.fullScreen = (ReadDword(hKey, L"Full Screen", cfg.fullScreen ? 1 : 0) != 0);

    cfg.useSound = (ReadDword(hKey, L"UseSound", cfg.useSound ? 1 : 0) != 0);
    cfg.soundFrequency = static_cast<int>(ReadDword(hKey, L"SoundFrequency", static_cast<DWORD>(cfg.soundFrequency)));
    cfg.soundBit = static_cast<int>(ReadDword(hKey, L"SoundBit", static_cast<DWORD>(cfg.soundBit)));
    cfg.soundStereo = (ReadDword(hKey, L"SoundStereo", cfg.soundStereo ? 1 : 0) != 0);
    cfg.sample2D = static_cast<int>(ReadDword(hKey, L"2DSample", static_cast<DWORD>(cfg.sample2D)));
    cfg.sample3D = static_cast<int>(ReadDword(hKey, L"3DSample", static_cast<DWORD>(cfg.sample3D)));
    cfg.streamSample = static_cast<int>(ReadDword(hKey, L"StreamSample", static_cast<DWORD>(cfg.streamSample)));

    cfg.use3dEffect = (ReadDword(hKey, L"Use3DEffect", cfg.use3dEffect ? 1 : 0) != 0);
    cfg.useMapEffect = (ReadDword(hKey, L"UseMapEffect", cfg.useMapEffect ? 1 : 0) != 0);

    cfg.captureFormat = ReadString(hKey, L"CaptureScreenFormat", cfg.captureFormat);
    cfg.captureQuality = ReadString(hKey, L"CaptureScreenJPGQuality", cfg.captureQuality);

    RegCloseKey(hKey);
    return cfg;
}

bool GameSettings::Save(const GameConfig& config) noexcept
{
    HKEY hKey = nullptr;
    DWORD disposition = 0;

    LONG status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        GetRegistrySubKey(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE | KEY_READ,
        nullptr,
        &hKey,
        &disposition);

    if (status != ERROR_SUCCESS || !hKey)
    {
        Logger::LogError("GameSettings: Failed to open or create registry key HKCU\\Software\\Trickster_NT.");
        return false;
    }

    bool success = true;
    success &= WriteDword(hKey, L"widthPixel", static_cast<DWORD>(config.widthPixel));
    success &= WriteDword(hKey, L"heightPixel", static_cast<DWORD>(config.heightPixel));
    success &= WriteDword(hKey, L"Full Screen", config.fullScreen ? 1 : 0);

    success &= WriteDword(hKey, L"UseSound", config.useSound ? 1 : 0);
    success &= WriteDword(hKey, L"SoundFrequency", static_cast<DWORD>(config.soundFrequency));
    success &= WriteDword(hKey, L"SoundBit", static_cast<DWORD>(config.soundBit));
    success &= WriteDword(hKey, L"SoundStereo", config.soundStereo ? 1 : 0);
    success &= WriteDword(hKey, L"2DSample", static_cast<DWORD>(config.sample2D));
    success &= WriteDword(hKey, L"3DSample", static_cast<DWORD>(config.sample3D));
    success &= WriteDword(hKey, L"StreamSample", static_cast<DWORD>(config.streamSample));

    success &= WriteDword(hKey, L"Use3DEffect", config.use3dEffect ? 1 : 0);
    success &= WriteDword(hKey, L"UseMapEffect", config.useMapEffect ? 1 : 0);

    success &= WriteString(hKey, L"CaptureScreenFormat", config.captureFormat);
    success &= WriteString(hKey, L"CaptureScreenJPGQuality", config.captureQuality);

    RegCloseKey(hKey);

    if (success)
    {
        Logger::Log("GameSettings: Successfully saved settings to HKCU\\Software\\Trickster_NT.");
    }
    else
    {
        Logger::LogError("GameSettings: Errors occurred while writing values to HKCU\\Software\\Trickster_NT.");
    }

    return success;
}
