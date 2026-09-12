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
    extern std::string WebsiteLink;
    extern bool IsCDNUsingSSL;

    // Authentication endpoint (launcher_auth.php)
    extern std::string AuthEndpointURL;
    extern std::string AuthToken;

    // Signed consolidated manifest migration.
    extern bool ManifestRequireSignature;
    extern std::string ManifestPublicKeyPem;
    // Certificate & Public-Key pinning for HTTPS
    extern std::vector<std::string> PinnedCertificateHashes;

    /// Load settings from config.json located in the given directory.
    /// If the file does not exist or cannot be parsed, compiled-in defaults are preserved.
    void Load(const std::wstring& exeDir) noexcept;
}
