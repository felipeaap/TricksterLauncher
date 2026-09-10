#pragma once

#include <functional>
#include <string>
#include <vector>

#include "DownloadManager.h"
#include "UpdateTypes.h"

class UpdateInstaller
{
public:
    using ProgressCallback = std::function<void(float fileProgress, float totalProgress)>;
    using MessageCallback = std::function<void(const std::string& fileName)>;
    using SpeedCallback = std::function<void(double bytesPerSecond)>;

    UpdateInstaller(std::vector<std::string> hosts,
                    bool useSsl,
                    DownloadManager::Options options = {});

    bool Install(const std::vector<Arquivo>& files,
                 int updateCount,
                 ProgressCallback progress = {},
                 MessageCallback message = {},
                 SpeedCallback speed = {}) const;

private:
    static bool EnsureDirectory(const std::string& path);
    static std::string DirectoryFromPath(const std::string& path);

    bool DownloadOne(const std::string& remotePath,
                     const std::string& localPath,
                     long long fileSize,
                     int fileIndex,
                     int totalFiles,
                     const ProgressCallback& progress,
                     const SpeedCallback& speed) const;

    std::vector<std::string> hosts_;
    bool useSsl_;
    DownloadManager::Options options_;
};
