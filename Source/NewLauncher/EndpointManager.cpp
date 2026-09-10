#include "EndpointManager.h"

#include <filesystem>
#include <utility>

namespace
{
void RemovePartialDownloadState(const std::string& localPath)
{
    std::error_code error;
    auto partPath = std::filesystem::path(localPath);
    partPath += ".part";
    std::filesystem::remove(partPath, error);
    error.clear();

    auto metadataPath = std::filesystem::path(localPath);
    metadataPath += ".part.meta";
    std::filesystem::remove(metadataPath, error);
}
}

EndpointManager::EndpointManager(std::vector<std::string> hosts,
                                 bool useSsl,
                                 DownloadManager::Options options)
    : hosts_(std::move(hosts)), useSsl_(useSsl), options_(options)
{
}

std::string EndpointManager::Get(const std::string& path) const
{
    for (const auto& host : hosts_)
    {
        if (host.empty())
            continue;

        DownloadManager manager(host, useSsl_, options_);
        const std::string result = manager.Get(path);
        if (!result.empty())
            return result;
    }

    return {};
}

bool EndpointManager::Download(const std::string& remotePath,
                               const std::string& localPath,
                               DownloadManager::ProgressCallback progress,
                               DownloadManager::SpeedCallback speed) const
{
    bool attempted = false;
    for (const auto& host : hosts_)
    {
        if (host.empty())
            continue;

        if (attempted)
            RemovePartialDownloadState(localPath);
        attempted = true;

        DownloadManager manager(host, useSsl_, options_);
        if (manager.Download(remotePath, localPath, progress, speed))
            return true;
    }

    return false;
}
