#include "Language.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>

#include "json.hpp"

namespace lang
{
namespace
{
    std::shared_mutex s_langMutex;

    const Dictionary kDefaultLanguages = 
    {
        {"splash_check",                 "Verifying file: "},
        {"launcher_waiting_server",      "Waiting for server response..."},
        {"launcher_checking",            "Connecting to server..."},
        {"launcher_worker_complete",     "Update complete!"},
        {"launcher_worker_maintenance",  "In Maintenance!"},
        {"launcher_worker_downloading",  "Downloading"},
        {"launcher_game_start",          "GAME START"},
        {"launcher_options",             "OPTION"},
        {"launcher_exit",                "EXIT"},
        {"launcher_verify",              "CHECK FILES"},
        {"launcher_check",               "CHECK FILES"},
        {"launcher_site_desc",           "You can visit our website by"},
        {"launcher_site_click",          "CLICKING HERE"},
        {"launcher_update_launch_fail",  "Failed to launch the updated launcher."},
        {"launcher_update_download_fail","Failed to download the updated launcher."},
        {"launcher_update_check_fail",   "Failed to check for launcher updates."},
        {"launcher_filelist_building",   "Creating update list..."},
        {"launcher_copy_fail",           "Failed to open the launcher!"},
        {"launcher_login_account",       "Account ID"},
        {"launcher_login_password",      "Password"},
        {"launcher_login_connect",       "CONNECT"},
        {"launcher_login_back",          "BACK"},
        {"launcher_login_remember",      "Remember ID"},
        {"settings_title",               "Game Configuration"},
        {"settings_tab_video",           "Video & Display"},
        {"settings_tab_audio",           "Audio"},
        {"settings_tab_graphics",        "Graphics & Capture"},
        {"settings_resolution",          "Screen Resolution"},
        {"settings_fullscreen",          "Full Screen"},
        {"settings_sound_enable",        "Enable Sound"},
        {"settings_sound_stereo",        "Stereo Audio (2 Channels)"},
        {"settings_sound_freq",          "Frequency"},
        {"settings_sound_bit",           "Bit Depth"},
        {"settings_sound_channels",      "Max Audio Channels"},
        {"settings_3d_effect",           "Enable 3D Effects"},
        {"settings_map_effect",          "Enable Map Effects"},
        {"settings_capture_format",      "Screenshot Format"},
        {"settings_capture_quality",     "JPEG Quality"},
        {"settings_save",                "SAVE"},
        {"settings_cancel",              "CANCEL"},
        {"settings_defaults",            "DEFAULTS"},
        {"settings_saved_success",       "Settings saved to registry!"},
    };
}

Dictionary Languages = kDefaultLanguages;

bool Load(const std::wstring& exeDir, const std::string& languageCode) noexcept
{
    try
    {
        std::unique_lock<std::shared_mutex> lock(s_langMutex);
        Languages = kDefaultLanguages; // Reset to default base before loading

        if (languageCode.empty() || languageCode == "en" || languageCode == "en-us")
        {
            // Already default English
        }

        const std::string langFile = languageCode + ".json";
        const std::wstring wLangFile(langFile.begin(), langFile.end());

        std::vector<std::filesystem::path> candidates;
        if (!exeDir.empty())
        {
            candidates.push_back(std::filesystem::path(exeDir) / L"LauncherData" / L"lang" / wLangFile);
            candidates.push_back(std::filesystem::path(exeDir) / L"lang" / wLangFile);
            candidates.push_back(std::filesystem::path(exeDir) / wLangFile);
        }
        candidates.push_back(std::filesystem::path(L"LauncherData/lang") / wLangFile);
        candidates.push_back(std::filesystem::path(L"lang") / wLangFile);
        candidates.push_back(std::filesystem::path(wLangFile));

        for (const auto& path : candidates)
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
                continue;

            std::ifstream file(path);
            if (!file.is_open())
                continue;

            const auto j = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
            if (j.is_discarded() || !j.is_object())
                continue;

            for (auto it = j.begin(); it != j.end(); ++it)
            {
                if (it.value().is_string())
                {
                    Languages[it.key()] = it.value().get<std::string>();
                }
            }
            return true;
        }
    }
    catch (...) {}

    return false;
}

std::string GetString(const std::string& key) noexcept 
{
    try
    {
        std::shared_lock<std::shared_mutex> lock(s_langMutex);
        auto it = Languages.find(key);
        if (it != Languages.end())
            return it->second;
    }
    catch (...) {}

    return "";
}

} // namespace lang