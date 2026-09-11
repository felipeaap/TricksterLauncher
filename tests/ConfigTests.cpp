#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Config.h"

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
        const char* json = R"({ "dll_inject": true, "dll_name": "Test.dll" })";
        const std::wstring dir = WriteTempConfig(json);
        const std::string cdnBefore = config::LauncherCDN;
        config::Load(dir);
        assert(config::IsDllInjectEnable == true);
        assert(config::InjectDLLName == "Test.dll");
        assert(config::LauncherCDN == cdnBefore && "Other fields must remain unchanged");
        RemoveTempConfig();
    }

    std::cout << "[PASS] Config tests passed!" << std::endl;
}
