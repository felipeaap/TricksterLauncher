#include "VersionManager.h"

#include <filesystem>
#include <fstream>

int VersionManager::Load(const char* path, int defaultVersion)
{
    std::filesystem::path targetPath(path);
    std::error_code ec;

    if (!std::filesystem::exists(targetPath, ec))
    {
        // Check for legacy root version.dat migration
        const std::filesystem::path legacyPath("version.dat");
        if (targetPath != legacyPath && std::filesystem::exists(legacyPath, ec))
        {
            std::ifstream legacyFile(legacyPath);
            if (legacyFile)
            {
                int legacyVer = defaultVersion;
                legacyFile >> legacyVer;
                legacyFile.close();
                if (!legacyFile.fail())
                {
                    Save(legacyVer, path);
                    std::filesystem::remove(legacyPath, ec);
                    return legacyVer;
                }
            }
        }
        return defaultVersion;
    }

    std::ifstream file(targetPath);
    if (!file)
        return defaultVersion;

    int version = defaultVersion;
    file >> version;

    return file.fail() ? defaultVersion : version;
}

bool VersionManager::Save(int version, const char* path)
{
    std::filesystem::path targetPath(path);
    std::error_code ec;

    const auto parent = targetPath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream file(targetPath, std::ios::trunc);
    if (!file)
        return false;

    file << version;
    return file.good();
}
