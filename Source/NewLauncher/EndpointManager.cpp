#include "EndpointManager.h"

#include <utility>

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
    for (const auto& host : hosts_)
    {
        if (host.empty())
            continue;

        DownloadManager manager(host, useSsl_, options_);
        if (manager.Download(remotePath, localPath, progress, speed))
            return true;
    }

    return false;
}
