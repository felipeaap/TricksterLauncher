#pragma once

#include <string>
#include <vector>

#include "Helper.h"

class ManifestManager
{
public:
    using FileList = std::vector<Arquivo>;

    explicit ManifestManager(Helper& helper);

    int Load(FileList& files, bool isFullCheck, int& localVersion);

private:
    Helper& helper_;

    static bool IsSamePath(const std::string& left, const std::string& right);
    static void MergeFile(FileList& files, const Arquivo& file);
};
