#include "Config.h"

#include <filesystem>
#include <fstream>

#include "json.hpp"

namespace config
{
    std::wstring WindowTitle            = L"Trickster Launcher (Dev Local)";
    std::string SubTitle                = "Trickster Launcher";
    std::string LauncherCDN             = "127.0.0.1:8000";
    std::vector<std::string> LauncherCDNBackups = {};
    std::wstring BaseNewsURL            = L"http://127.0.0.1:8000/news.html";
    std::string WebsiteLink             = "http://127.0.0.1:8000";
    std::string OptionExecName          = "Setup.exe";
    bool IsCDNUsingSSL                  = false;
    bool IsDllInjectEnable              = false;
    std::string InjectDLLName           = "Trickster.dll";
    bool ManifestRequireSignature       = false;
    std::string ManifestPublicKeyPem    = {};
    std::vector<std::string> PinnedCertificateHashes = {};

    void Load(const std::wstring& exeDir) noexcept
    {
        try
        {
            const std::filesystem::path configPath =
                std::filesystem::path(exeDir) / L"config.json";

            if (!std::filesystem::exists(configPath))
                return;

            std::ifstream file(configPath);
            if (!file.is_open())
                return;

            const auto j = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
            if (j.is_discarded())
                return;

            auto getString = [&](const char* key, std::string& out)
            {
                if (j.contains(key) && j[key].is_string())
                    out = j[key].get<std::string>();
            };
            auto getBool = [&](const char* key, bool& out)
            {
                if (j.contains(key) && j[key].is_boolean())
                    out = j[key].get<bool>();
            };
            auto getWString = [&](const char* key, std::wstring& out)
            {
                if (j.contains(key) && j[key].is_string())
                {
                    const auto s = j[key].get<std::string>();
                    out = std::wstring(s.begin(), s.end());
                }
            };

            getWString("window_title",  WindowTitle);
            getString ("subtitle",      SubTitle);
            getString ("cdn",           LauncherCDN);
            getWString("news_url",      BaseNewsURL);
            getString ("website_link",  WebsiteLink);
            getString ("option_exec",   OptionExecName);
            getBool   ("use_ssl",       IsCDNUsingSSL);
            getBool   ("dll_inject",    IsDllInjectEnable);
            getString ("dll_name",      InjectDLLName);
            getBool   ("manifest_require_signature", ManifestRequireSignature);
            getString ("manifest_public_key_pem",    ManifestPublicKeyPem);

            if (j.contains("pinned_cert_hashes") && j["pinned_cert_hashes"].is_array())
            {
                PinnedCertificateHashes.clear();
                for (const auto& item : j["pinned_cert_hashes"])
                    if (item.is_string())
                        PinnedCertificateHashes.push_back(item.get<std::string>());
            }

            if (j.contains("cdn_backups") && j["cdn_backups"].is_array())
            {
                LauncherCDNBackups.clear();
                for (const auto& item : j["cdn_backups"])
                    if (item.is_string())
                        LauncherCDNBackups.push_back(item.get<std::string>());
            }
        }
        catch (...) {}
    }
}
