#pragma once

#include <string>
#include <vector>

struct ResolutionOption
{
    int width;
    int height;
    std::string label;
};

struct GameConfig
{
    int widthPixel = 1024;
    int heightPixel = 768;
    bool fullScreen = false;

    bool useSound = true;
    int soundFrequency = 44100; // 22050 or 44100
    int soundBit = 16;          // 8 or 16
    bool soundStereo = true;    // true = Stereo (1), false = Mono (0)
    int sample2D = 32;          // 8, 16, 32, 48
    int sample3D = 32;          // 8, 16, 32, 48
    int streamSample = 32;      // 8, 16, 32, 48

    bool use3dEffect = true;
    bool useMapEffect = true;

    std::string captureFormat = "JPG";            // "JPG" or "BMP"
    std::string captureQuality = "Very High";     // "Low", "Middle", "High", "Very High"
};

class GameSettings
{
public:
    static const wchar_t* GetRegistrySubKey() noexcept { return L"Software\\Trickster_NT"; }

    static GameConfig GetDefaults() noexcept;
    static GameConfig Load() noexcept;
    static bool Save(const GameConfig& config) noexcept;

    static const std::vector<ResolutionOption>& GetSupportedResolutions() noexcept;
};
