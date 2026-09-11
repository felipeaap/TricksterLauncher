#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "DownloadManager.h"

class LauncherUpdater
{
public:
    LauncherUpdater(std::vector<std::string> hosts,
                    bool useSsl,
                    DownloadManager::Options options = {});

    bool Update(const std::filesystem::path& launcherPath,
                const std::string& remoteHash,
                const std::string& remotePath,
                const std::string& currentExecutable) const;

private:
    std::vector<std::string> hosts_;
    bool useSsl_;
    DownloadManager::Options options_;
};
