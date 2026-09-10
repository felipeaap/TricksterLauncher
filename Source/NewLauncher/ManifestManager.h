#pragma once

#include <functional>
#include <string>
#include <vector>

#include "UpdateTypes.h"

class ManifestManager
{
public:
    using FileList = std::vector<Arquivo>;
    using FetchFunction = std::function<std::string(const std::string&)>;

    explicit ManifestManager(FetchFunction fetch);

    int Load(FileList& files, bool isFullCheck, int& localVersion);

private:
    FetchFunction fetch_;

    static bool IsSamePath(const std::string& left, const std::string& right);
    static void MergeFile(FileList& files, const Arquivo& file);
};
