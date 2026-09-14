#pragma once
#include <string>
#include <vector>

namespace config
{
    extern std::wstring WindowTitle;
    extern std::string SubTitle;
    extern std::string LauncherCDN;
    extern std::vector<std::string> LauncherCDNBackups;
    extern std::string OptionExecName;
    extern std::string GameExecName;
    extern std::string Region;
    extern std::string Language;
    extern bool IsCDNUsingSSL;

    // Authentication endpoint (auth.php / launcher_auth.php)
    extern std::string AuthEndpointURL;
    extern std::string AuthToken;
    extern std::string SavedAccount;
    extern bool RememberAccount;

    /// Automatically resolves the auth endpoint URL from LauncherCDN if not explicitly configured.
    std::string GetResolvedAuthEndpointURL() noexcept;

    // Signed consolidated manifest migration.
    extern bool ManifestRequireSignature;
    extern std::string ManifestPublicKeyPem;
    // Certificate & Public-Key pinning for HTTPS
    extern std::vector<std::string> PinnedCertificateHashes;

    /// Load settings from config.json located in the given directory.
    /// If the file does not exist or cannot be parsed, compiled-in defaults are preserved.
    void Load(const std::wstring& exeDir) noexcept;

    /// Save persistent user settings (e.g. saved_account, remember_account) to config.json.
    void Save(const std::wstring& exeDir) noexcept;
}
