#include "ManifestManager.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "json.hpp"

ManifestManager::ManifestManager(FetchFunction fetch)
    : fetch_(std::move(fetch))
{
}

int ManifestManager::Load(FileList& files, bool isFullCheck, int& localVersion)
{
    localVersion = isFullCheck ? 1 : localVersion;
    files.clear();

    int currentVersion = localVersion;
    while (true)
    {
        const std::string path = "/version/version_" + std::to_string(currentVersion) + ".json";
        const std::string jsonContent = fetch_(path);
        if (jsonContent.empty())
            break;

        const nlohmann::json document = nlohmann::json::parse(jsonContent);
        if (document.is_array())
        {
            for (const auto& item : document)
            {
                Arquivo file;
                file.FileID = item.value("FileID", 0);
                file.FileHash = item.value("FileHash", "");
                file.FilePath = item.value("FilePath", "");
                MergeFile(files, file);
            }
        }

        ++currentVersion;
    }

    return currentVersion;
}

bool ManifestManager::IsSamePath(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;

    for (size_t i = 0; i < left.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i])))
        {
            return false;
        }
    }

    return true;
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
