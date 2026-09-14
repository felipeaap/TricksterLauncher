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
    std::string GameExecName            = "Trickster/trickster.bin";
    std::string Region                  = "thailand";
    std::string Language                = "en-us";
    bool IsCDNUsingSSL                  = false;
    std::string AuthEndpointURL         = "";
    std::string AuthToken               = "";
    std::string SavedAccount            = "";
    bool RememberAccount                = true;
    bool ManifestRequireSignature       = false;
    std::string ManifestPublicKeyPem    = {};
    std::vector<std::string> PinnedCertificateHashes = {};

    std::string GetResolvedAuthEndpointURL() noexcept
    {
        if (!AuthEndpointURL.empty())
            return AuthEndpointURL;

        if (LauncherCDN.empty())
            return "";

        std::string url;
        if (LauncherCDN.rfind("http://", 0) == 0 || LauncherCDN.rfind("https://", 0) == 0)
        {
            url = LauncherCDN;
        }
        else
        {
            url = (IsCDNUsingSSL ? "https://" : "http://") + LauncherCDN;
        }

        if (url.back() != '/')
            url += '/';

        url += "auth.php";
        return url;
    }

    void Load(const std::wstring& exeDir) noexcept
    {
        try
        {
            std::error_code ec;
            std::filesystem::path configPath =
                std::filesystem::path(exeDir) / L"LauncherData" / L"config.json";

            if (!std::filesystem::exists(configPath, ec))
            {
                // Fallback to legacy root config.json
                const std::filesystem::path legacyConfig =
                    std::filesystem::path(exeDir) / L"config.json";
                if (std::filesystem::exists(legacyConfig, ec))
                {
                    configPath = legacyConfig;
                }
                else
                {
                    return;
                }
            }

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
            getString ("game_exec",     GameExecName);
            getString ("region",        Region);
            getString ("language",      Language);
            getBool   ("use_ssl",       IsCDNUsingSSL);
            getBool   ("manifest_require_signature", ManifestRequireSignature);
            getString ("manifest_public_key_pem",    ManifestPublicKeyPem);
            getString ("auth_endpoint_url",          AuthEndpointURL);
            getString ("auth_token",                 AuthToken);
            getString ("saved_account",              SavedAccount);
            getBool   ("remember_account",           RememberAccount);

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

    void Save(const std::wstring& exeDir) noexcept
    {
        try
        {
            std::filesystem::path configPath =
                std::filesystem::path(exeDir) / L"LauncherData" / L"config.json";

            nlohmann::json j;
            {
                std::ifstream inFile(configPath);
                if (inFile.is_open())
                {
                    j = nlohmann::json::parse(inFile, nullptr, false);
                }
            }
            if (j.is_discarded() || !j.is_object())
            {
                j = nlohmann::json::object();
            }

            if (RememberAccount)
            {
                j["saved_account"] = SavedAccount;
                j["remember_account"] = true;
            }
            else
            {
                j["saved_account"] = "";
                j["remember_account"] = false;
            }

            std::filesystem::create_directories(configPath.parent_path());
            std::ofstream outFile(configPath);
            if (outFile.is_open())
            {
                outFile << j.dump(4);
            }
        }
        catch (...) {}
    }
}
