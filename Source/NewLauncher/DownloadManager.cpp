#include "DownloadManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>

#include "httplib.h"

namespace
{
template <typename Client>
std::string GetImpl(Client& client, const std::string& path, int timeoutSeconds)
{
    client.set_follow_location(true);
    client.set_connection_timeout(timeoutSeconds, 0);

    const auto response = client.Get(path.c_str());
    if (!response || response->status != 200)
        return {};

    return response->body;
}

template <typename Client>
bool DownloadAttempt(Client& client,
                     const std::string& remotePath,
                     const std::filesystem::path& temporaryPath,
                     const DownloadManager::ProgressCallback& progress,
                     const DownloadManager::SpeedCallback& speedCallback,
                     int timeoutSeconds)
{
    client.set_follow_location(true);
    client.set_connection_timeout(timeoutSeconds, 0);

    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
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
            {
                try
                {
                    contentLength = std::stoll(it->second);
                }
                catch (...)
                {
                    contentLength = 0;
                }
            }
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

    output.flush();
    output.close();

    if (!response || response->status != 200)
        return false;

    if (contentLength > 0 && downloaded != contentLength)
        return false;

    return downloaded > 0 || contentLength == 0;
}
}

DownloadManager::DownloadManager(std::string host, bool useSsl, Options options)
    : host_(std::move(host)), useSsl_(useSsl), options_(options)
{
    if (options_.maxRetries < 0)
        options_.maxRetries = 0;
    if (options_.connectionTimeoutSeconds <= 0)
        options_.connectionTimeoutSeconds = 5;
    if (options_.retryDelayMilliseconds < 0)
        options_.retryDelayMilliseconds = 0;
}

std::string DownloadManager::Get(const std::string& path) const
{
    if (useSsl_)
    {
        httplib::SSLClient client(host_.c_str());
        return GetImpl(client, path, options_.connectionTimeoutSeconds);
    }

    httplib::Client client(host_.c_str());
    return GetImpl(client, path, options_.connectionTimeoutSeconds);
}

bool DownloadManager::Download(const std::string& remotePath,
                               const std::string& localPath,
                               ProgressCallback progress,
                               SpeedCallback speed) const
{
    const std::filesystem::path destination(localPath);
    std::filesystem::path temporaryPath = destination;
    temporaryPath += ".tmp";

    std::error_code error;
    if (std::filesystem::exists(temporaryPath, error))
        std::filesystem::remove(temporaryPath, error);

    for (int attempt = 0; attempt <= options_.maxRetries; ++attempt)
    {
        bool success = false;

        if (useSsl_)
        {
            httplib::SSLClient client(host_.c_str());
            success = DownloadAttempt(client,
                                      remotePath,
                                      temporaryPath,
                                      progress,
                                      speed,
                                      options_.connectionTimeoutSeconds);
        }
        else
        {
            httplib::Client client(host_.c_str());
            success = DownloadAttempt(client,
                                      remotePath,
                                      temporaryPath,
                                      progress,
                                      speed,
                                      options_.connectionTimeoutSeconds);
        }

        if (success)
        {
            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::rename(temporaryPath, destination, error);
            if (!error)
                return true;
        }

        std::filesystem::remove(temporaryPath, error);

        if (attempt < options_.maxRetries && options_.retryDelayMilliseconds > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(options_.retryDelayMilliseconds));
        }
    }

    std::filesystem::remove(temporaryPath, error);
    return false;
}
