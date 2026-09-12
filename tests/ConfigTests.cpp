#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Config.h"
#include "Language.h"

namespace
{
    // Write a temporary config.json to the system temp directory and return its directory.
    std::wstring WriteTempConfig(const char* json)
    {
        const std::filesystem::path tmpDir =
            std::filesystem::temp_directory_path() / "TricksterLauncherTests";
        std::filesystem::create_directories(tmpDir);

        const std::filesystem::path configPath = tmpDir / "config.json";
        std::ofstream f(configPath);
        f << json;
        return tmpDir.wstring();
    }

    void RemoveTempConfig()
    {
        const std::filesystem::path tmpDir =
            std::filesystem::temp_directory_path() / "TricksterLauncherTests";
        std::filesystem::remove_all(tmpDir);
    }
}

void RunConfigTests()
{
    std::cout << "[TEST] Running Config tests..." << std::endl;

    // ── Test 1: Fallback to defaults when config.json does not exist ─────────
    {
        const std::wstring noSuchDir = L"C:\\DoesNotExist_TricksterTest\\";
        const std::string origCDN = config::LauncherCDN;
        config::Load(noSuchDir);
        assert(config::LauncherCDN == origCDN && "Config::Load should keep defaults when file missing");
    }

    // ── Test 2: Values are overridden from a valid JSON file ─────────────────
    {
        const char* json = R"({
            "cdn": "myserver.example.com:8080",
            "subtitle": "Test Subtitle",
            "use_ssl": true,
            "cdn_backups": ["backup1.com", "backup2.com"],
            "option_exec": "Options.exe"
        })";
        const std::wstring dir = WriteTempConfig(json);
        config::Load(dir);

        assert(config::LauncherCDN == "myserver.example.com:8080");
        assert(config::SubTitle == "Test Subtitle");
        assert(config::IsCDNUsingSSL == true);
        assert(config::LauncherCDNBackups.size() == 2);
        assert(config::LauncherCDNBackups[0] == "backup1.com");
        assert(config::LauncherCDNBackups[1] == "backup2.com");
        assert(config::OptionExecName == "Options.exe");

        RemoveTempConfig();
    }

    // ── Test 3: Malformed JSON does not crash and keeps current values ────────
    {
        const char* brokenJson = "{ invalid json !!!! }";
        const std::wstring dir = WriteTempConfig(brokenJson);
        const std::string cdnBefore = config::LauncherCDN;
        config::Load(dir);
        assert(config::LauncherCDN == cdnBefore && "Malformed JSON must not change existing config");
        RemoveTempConfig();
    }

    // ── Test 4: Partial JSON only updates specified fields ───────────────────
    {
        const char* json = R"({ "region": "japan", "game_exec": "Bin/game.bin" })";
        const std::wstring dir = WriteTempConfig(json);
        const std::string cdnBefore = config::LauncherCDN;
        config::Load(dir);
        assert(config::Region == "japan");
        assert(config::GameExecName == "Bin/game.bin");
        assert(config::LauncherCDN == cdnBefore && "Other fields must remain unchanged");
        RemoveTempConfig();
    }

    // ── Test 5: Pinned cert hashes parsing ─────────────────────────────────
    {
        const char* json = R"({
            "pinned_cert_hashes": ["a1b2c3d4e5f6", "112233445566"]
        })";
        const std::wstring dir = WriteTempConfig(json);
        config::Load(dir);
        assert(config::PinnedCertificateHashes.size() == 2);
        assert(config::PinnedCertificateHashes[0] == "a1b2c3d4e5f6");
        assert(config::PinnedCertificateHashes[1] == "112233445566");
        RemoveTempConfig();
    }

    // ── Test 6: LauncherData/config.json subfolder loading ─────────────────
    {
        const std::filesystem::path tmpDir =
            std::filesystem::temp_directory_path() / "TricksterLauncherTests_Subfolder";
        const std::filesystem::path dataDir = tmpDir / "LauncherData";
        std::filesystem::create_directories(dataDir);

        const std::filesystem::path configPath = dataDir / "config.json";
        std::ofstream f(configPath);
        f << R"({ "cdn": "subfolder.cdn.com", "option_exec": "apps/Setup.exe" })";
        f.close();

        config::Load(tmpDir.wstring());
        assert(config::LauncherCDN == "subfolder.cdn.com");
        assert(config::OptionExecName == "apps/Setup.exe");

        std::filesystem::remove_all(tmpDir);
    }

    // ── Test 7: Language (i18n) loading & fallback ─────────────────────────
    {
        const std::filesystem::path tmpDir =
            std::filesystem::temp_directory_path() / "TricksterLauncherTests_Lang";
        const std::filesystem::path langDir = tmpDir / "LauncherData" / "lang";
        std::filesystem::create_directories(langDir);

        const std::filesystem::path ptFile = langDir / "pt-br.json";
        std::ofstream f(ptFile);
        f << R"({ "launcher_game_start": "INICIAR JOGO", "launcher_options": "OPÇÕES" })";
        f.close();

        // Default before custom load
        lang::Load(tmpDir.wstring(), "en-us");
        assert(lang::GetString("launcher_game_start") == "GAME START");

        // Load custom pt-br
        bool loaded = lang::Load(tmpDir.wstring(), "pt-br");
        assert(loaded == true);
        assert(lang::GetString("launcher_game_start") == "INICIAR JOGO");
        assert(lang::GetString("launcher_options") == "OPÇÕES");
        // Fallback for unprovided key
        assert(lang::GetString("launcher_exit") == "EXIT");

        // Reset back to en-us
        lang::Load(tmpDir.wstring(), "en-us");
        assert(lang::GetString("launcher_game_start") == "GAME START");

        std::filesystem::remove_all(tmpDir);
    }

    std::cout << "[PASS] Config and Language (i18n) tests passed!" << std::endl;
}
