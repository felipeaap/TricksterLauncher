#pragma once
#include <string>
#include <vector>

namespace config
{
    extern std::wstring WindowTitle;
    extern std::string SubTitle;
    extern std::string LauncherCDN;
    extern std::vector<std::string> LauncherCDNBackups;
    extern std::wstring BaseNewsURL;
    extern std::string OptionExecName;
    extern std::string WebsiteLink;
    extern bool IsCDNUsingSSL;
    extern bool IsDllInjectEnable;
    extern std::string InjectDLLName;

    // Signed consolidated manifest migration.
    extern bool ManifestRequireSignature;
    extern std::string ManifestPublicKeyPem;

    /// Load settings from config.json located in the given directory.
    /// If the file does not exist or cannot be parsed, compiled-in defaults are preserved.
    void Load(const std::wstring& exeDir) noexcept;
}
