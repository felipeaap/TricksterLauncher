#include "VersionManager.h"

#include <fstream>

int VersionManager::Load(const char* path, int defaultVersion)
{
    std::ifstream file(path);
    if (!file)
        return defaultVersion;

    int version = defaultVersion;
    file >> version;

    return file.fail() ? defaultVersion : version;
}

bool VersionManager::Save(int version, const char* path)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
        return false;

    file << version;
    return file.good();
}
