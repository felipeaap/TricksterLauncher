#include "Config.h"

namespace config
{
    std::wstring WindowTitle    = L"Trickster Launcher";
    std::string SubTitle        = "Trickster Launcher";
    std::string LauncherCDN     = "cdn.selenoid.com.br";
    std::vector<std::string> LauncherCDNBackups = {};
    std::wstring BaseNewsURL    = L"https://google.com.br";
    std::string WebsiteLink     = "https://google.com";
    std::string OptionExecName  = "Setup.exe";
    bool IsCDNUsingSSL          = true;
    bool IsDllInjectEnable      = false;
    std::string InjectDLLName   = "Trickster.dll";
    bool ManifestRequireSignature = false;
    std::string ManifestPublicKeyPem = {};
}
