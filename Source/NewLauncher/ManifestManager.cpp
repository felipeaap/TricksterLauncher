#include "ManifestManager.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "Config.h"
#include "ManifestSecurity.h"
#include "json.hpp"

namespace
{
std::string CanonicalManifestPayload(const nlohmann::json& document)
{
    nlohmann::json payload = document;
    payload.erase("signature");
    return payload.dump();
}

bool ValidateConsolidatedManifest(const nlohmann::json& document)
{
    const auto signature = document.find("signature");
    if (signature == document.end() || !signature->is_object())
        return !config::ManifestRequireSignature;

    const std::string value = signature->value("value", "");
    if (value.empty() || config::ManifestPublicKeyPem.empty())
        return false;

    return manifest_security::Verify(
        CanonicalManifestPayload(document),
        value,
        config::ManifestPublicKeyPem);
}

bool SamePathIgnoreCase(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;

    for (size_t i = 0; i < left.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i])))
            return false;
    }

    return true;
}

void LoadFilesFromArray(const nlohmann::json& array, ManifestManager::FileList& files)
{
    if (!array.is_array())
        return;

    for (const auto& item : array)
    {
        Arquivo file;
        file.FileID = item.value("FileID", 0);
        file.FileHash = item.value("FileHash", "");
        file.FilePath = item.value("FilePath", "");
        file.ToUpdate = item.value("ToUpdate", false);
        file.FileSize = item.value("FileSize", 0LL);

        const auto it = std::find_if(files.begin(), files.end(),
            [&](const Arquivo& current) { return SamePathIgnoreCase(current.FilePath, file.FilePath); });

        if (it != files.end())
            *it = file;
        else
            files.push_back(file);
    }
}
}

ManifestManager::ManifestManager(FetchFunction fetch)
    : fetch_(std::move(fetch))
{
}

int ManifestManager::Load(FileList& files, bool isFullCheck, int& localVersion)
{
    const std::string consolidated = fetch_ ? fetch_("/manifest.json") : std::string();
    return Parse(consolidated, files, isFullCheck, localVersion);
}

int ManifestManager::Parse(const std::string& consolidated, FileList& files, bool isFullCheck, int& localVersion)
{
    localVersion = isFullCheck ? 1 : localVersion;
    files.clear();

    if (consolidated.empty())
        return localVersion;

    try
    {
        const nlohmann::json document = nlohmann::json::parse(consolidated);
        if (ValidateConsolidatedManifest(document))
        {
            const int manifestVersion = document.value("version", localVersion);

            // Lazy fast-path: When up-to-date and not full-checking, skip heavy array allocation & file parsing
            if (!isFullCheck && localVersion > 0 && manifestVersion > 0 && localVersion >= manifestVersion)
            {
                return manifestVersion;
            }

            LoadFilesFromArray(document.value("files", nlohmann::json::array()), files);
            return manifestVersion;
        }
        else
        {
            files.clear();
            return localVersion;
        }
    }
    catch (const nlohmann::json::exception&)
    {
        files.clear();
        return localVersion;
    }
}

bool ManifestManager::IsSamePath(const std::string& left, const std::string& right)
{
    return SamePathIgnoreCase(left, right);
}

void ManifestManager::MergeFile(FileList& files, const Arquivo& file)
{
    const auto it = std::find_if(files.begin(), files.end(),
        [&](const Arquivo& current) { return IsSamePath(current.FilePath, file.FilePath); });

    if (it != files.end())
        *it = file;
    else
        files.push_back(file);
}
