#pragma once

#include <string>
#include <vector>

#include "DownloadManager.h"

class EndpointManager
{
public:
    EndpointManager(std::vector<std::string> hosts,
                    bool useSsl,
                    DownloadManager::Options options = {});

    std::string Get(const std::string& path) const;
    bool Download(const std::string& remotePath,
                  const std::string& localPath,
                  DownloadManager::ProgressCallback progress = {},
                  DownloadManager::SpeedCallback speed = {},
                  const std::string& expectedHash = {}) const;

private:
    std::vector<std::string> hosts_;
    bool useSsl_;
    DownloadManager::Options options_;
};
