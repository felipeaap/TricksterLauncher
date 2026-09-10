#include "DownloadManager.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

#include "httplib.h"

namespace
{
template <typename Client>
std::string GetImpl(Client& client, const std::string& path)
{
    client.set_follow_location(true);
    client.set_connection_timeout(5, 0);

    const auto response = client.Get(path.c_str());
    if (!response || response->status != 200)
        return {};

    return response->body;
}

template <typename Client>
bool DownloadImpl(Client& client,
                  const std::string& remotePath,
                  const std::string& localPath,
                  const DownloadManager::ProgressCallback& progress,
                  const DownloadManager::SpeedCallback& speedCallback)
{
    client.set_follow_location(true);
    client.set_connection_timeout(5, 0);

    std::ofstream output(localPath, std::ios::binary);
    if (!output)
        return false;

    long long downloaded = 0;
    long long contentLength = 0;
    const auto startTime = std::chrono::steady_clock::now();

    const auto response = client.Get(("/Update/" + remotePath).c_str(),
        [&](const httplib::Response& responseHeader)
        {
            const auto it = responseHeader.headers.find("Content-Length");
            if (it != responseHeader.headers.end())
                contentLength = std::stoll(it->second);
            return true;
        },
        [&](const char* data, size_t length)
        {
            output.write(data, static_cast<std::streamsize>(length));
            if (!output)
                return false;

            downloaded += static_cast<long long>(length);
            if (progress)
                progress(downloaded, contentLength);

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - startTime).count();
            if (speedCallback)
                speedCallback(elapsed > 0.0 ? downloaded / elapsed : 0.0);

            return true;
        });

    return response && response->status == 200;
}
}

DownloadManager::DownloadManager(std::string host, bool useSsl)
    : host_(std::move(host)), useSsl_(useSsl)
{
}

std::string DownloadManager::Get(const std::string& path) const
{
    if (useSsl_)
    {
        httplib::SSLClient client(host_.c_str());
        return GetImpl(client, path);
    }

    httplib::Client client(host_.c_str());
    return GetImpl(client, path);
}

bool DownloadManager::Download(const std::string& remotePath,
                               const std::string& localPath,
                               ProgressCallback progress,
                               SpeedCallback speed) const
{
    if (useSsl_)
    {
        httplib::SSLClient client(host_.c_str());
        return DownloadImpl(client, remotePath, localPath, progress, speed);
    }

    httplib::Client client(host_.c_str());
    return DownloadImpl(client, remotePath, localPath, progress, speed);
}
