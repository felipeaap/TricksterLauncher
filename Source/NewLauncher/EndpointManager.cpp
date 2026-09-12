#include "EndpointManager.h"

#include <chrono>
#include <filesystem>
#include <utility>

#include "DownloadTelemetry.h"
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

bool EndpointManager::Fetch(const std::string& path, std::string& outBody) const
{
    const auto rankedHosts = download_telemetry::GetRankedHosts(hosts_);
    for (const auto& host : rankedHosts)
    {
        if (host.empty())
            continue;

        DownloadManager manager(host, useSsl_, options_);
        if (manager.Fetch(path, outBody))
            return true;
    }

    outBody.clear();
    return false;
}

std::string EndpointManager::Get(const std::string& path) const
{
    std::string result;
    Fetch(path, result);
    return result;
}

bool EndpointManager::Download(const std::string& remotePath,
                               const std::string& localPath,
                               DownloadManager::ProgressCallback progress,
                               DownloadManager::SpeedCallback speed,
                               const std::string& expectedHash) const
{
    const auto rankedHosts = download_telemetry::GetRankedHosts(hosts_);
    for (const auto& host : rankedHosts)
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

