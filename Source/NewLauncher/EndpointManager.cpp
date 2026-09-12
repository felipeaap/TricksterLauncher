#include "EndpointManager.h"

#include <chrono>
#include <filesystem>
#include <utility>

#include "Logger.h"

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
                               DownloadManager::SpeedCallback speed,
                               const std::string& expectedHash) const
{
    for (const auto& host : hosts_)
    {
        if (host.empty())
            continue;

        DownloadManager manager(host, useSsl_, options_);
        const bool success = manager.Download(
            remotePath,
            localPath,
            progress,
            speed,
            [host](const std::string& err)
            {
                Logger::LogError("[" + host + "] " + err);
            },
            expectedHash);

        if (success)
            return true;
    }

    return false;
}
