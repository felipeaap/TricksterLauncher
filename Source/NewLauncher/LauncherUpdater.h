#pragma once

#include <filesystem>
#include <string>

#include "DownloadManager.h"

class LauncherUpdater
{
public:
    struct Callbacks
    {
        std::string remoteHash;
        std::string localPath;
        std::string remotePath;
    };

    LauncherUpdater(std::string host, bool useSsl, DownloadManager::Options options = {});

    bool Update(const std::filesystem::path& launcherPath,
                const std::string& remoteHash,
                const std::string& remotePath,
                const std::string& currentExecutable) const;

private:
    std::string host_;
    bool useSsl_;
    DownloadManager::Options options_;
};
