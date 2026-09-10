#include "DownloadManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "httplib.h"

namespace
{
struct RemoteInfo
{
    long long size = 0;
    bool rangeSupported = false;
};

struct SegmentState
{
    long long start = 0;
    long long end = 0;
    bool complete = false;
};

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

bool ParseContentRange(const std::string& value, long long& total)
{
    const auto slash = value.find('/');
    if (slash == std::string::npos)
        return false;

    try
    {
        total = std::stoll(value.substr(slash + 1));
    }
    catch (...)
    {
        return false;
    }

    return total > 0;
}

template <typename Client>
RemoteInfo ProbeRemote(Client& client,
                       const std::string& remotePath,
                       int timeoutSeconds)
{
    RemoteInfo info;
    client.set_follow_location(true);
    client.set_connection_timeout(timeoutSeconds, 0);

    const std::string url = "/Update/" + remotePath;
    const auto head = client.Head(url.c_str());
    if (head)
    {
        const auto length = head->headers.find("Content-Length");
        if (length != head->headers.end())
        {
            try
            {
                info.size = std::stoll(length->second);
            }
            catch (...)
            {
                info.size = 0;
            }
        }

        const auto acceptRanges = head->headers.find("Accept-Ranges");
        if (acceptRanges != head->headers.end())
        {
            std::string value = acceptRanges->second;
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            info.rangeSupported = value.find("bytes") != std::string::npos;
        }
    }

    // A number of CDNs omit Accept-Ranges on HEAD. Probe one byte while
    // discarding the body to avoid buffering a large file when Range is ignored.
    if (!info.rangeSupported)
    {
        httplib::Headers headers;
        headers.emplace("Range", "bytes=0-0");

        const auto response = client.Get(url.c_str(), headers,
            [](const char*, size_t)
            {
                return false;
            });

        if (response && response->status == 206)
        {
            const auto range = response->headers.find("Content-Range");
            if (range != response->headers.end())
                info.rangeSupported = ParseContentRange(range->second, info.size);
        }
    }

    return info;
}

std::filesystem::path MakePartPath(const std::filesystem::path& destination)
{
    auto path = destination;
    path += ".part";
    return path;
}

std::filesystem::path MakeMetadataPath(const std::filesystem::path& destination)
{
    auto path = destination;
    path += ".part.meta";
    return path;
}

bool PreparePartFile(const std::filesystem::path& path, long long size)
{
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, error);

    std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file)
        return false;

    if (size > 0)
    {
        file.seekp(size - 1);
        char zero = 0;
        file.write(&zero, 1);
    }

    return file.good();
}

bool LoadMetadata(const std::filesystem::path& metadataPath,
                  long long remoteSize,
                  long long segmentSize,
                  std::vector<SegmentState>& segments)
{
    std::ifstream input(metadataPath);
    if (!input)
        return false;

    long long storedSize = 0;
    long long storedSegmentSize = 0;
    size_t count = 0;
    if (!(input >> storedSize >> storedSegmentSize >> count))
        return false;

    if (storedSize != remoteSize || storedSegmentSize != segmentSize || count == 0)
        return false;

    segments.clear();
    segments.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        int complete = 0;
        if (!(input >> complete))
            return false;

        SegmentState state;
        state.start = static_cast<long long>(i) * segmentSize;
        state.end = std::min(remoteSize - 1, state.start + segmentSize - 1);
        state.complete = complete != 0;
        segments.push_back(state);
    }

    return true;
}

bool SaveMetadata(const std::filesystem::path& metadataPath,
                  long long remoteSize,
                  long long segmentSize,
                  const std::vector<SegmentState>& segments)
{
    const auto temporaryPath = metadataPath.string() + ".tmp";
    std::ofstream output(temporaryPath, std::ios::trunc);
    if (!output)
        return false;

    output << remoteSize << '\n' << segmentSize << '\n' << segments.size() << '\n';
    for (const auto& segment : segments)
        output << (segment.complete ? 1 : 0) << '\n';

    output.flush();
    output.close();
    if (!output)
        return false;

    std::error_code error;
    std::filesystem::remove(metadataPath, error);
    error.clear();
    std::filesystem::rename(temporaryPath, metadataPath, error);
    return !error;
}

template <typename Client>
bool DownloadRange(Client& client,
                   const std::string& remotePath,
                   const std::filesystem::path& partPath,
                   long long start,
                   long long end,
                   std::atomic<long long>& downloadedTotal,
                   long long totalSize,
                   const DownloadManager::ProgressCallback& progress,
                   const DownloadManager::SpeedCallback& speedCallback,
                   const std::chrono::steady_clock::time_point& overallStart,
                   int timeoutSeconds)
{
    if (start > end)
        return true;

    client.set_follow_location(true);
    client.set_connection_timeout(timeoutSeconds, 0);

    httplib::Headers headers;
    headers.emplace("Range", "bytes=" + std::to_string(start) + "-" + std::to_string(end));

    std::fstream output(partPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!output)
        return false;

    output.seekp(start);
    if (!output)
        return false;

    long long segmentDownloaded = 0;
    const long long expected = end - start + 1;

    const auto response = client.Get(("/Update/" + remotePath).c_str(), headers,
        [&](const char* data, size_t length)
        {
            if (segmentDownloaded + static_cast<long long>(length) > expected)
                return false;

            output.write(data, static_cast<std::streamsize>(length));
            if (!output)
                return false;

            segmentDownloaded += static_cast<long long>(length);
            downloadedTotal.fetch_add(static_cast<long long>(length), std::memory_order_relaxed);

            if (progress)
            {
                const long long current = downloadedTotal.load(std::memory_order_relaxed);
                progress(current, totalSize);
            }

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - overallStart).count();
            if (speedCallback)
            {
                const long long current = downloadedTotal.load(std::memory_order_relaxed);
                speedCallback(elapsed > 0.0 ? current / elapsed : 0.0);
            }

            return true;
        });

    output.flush();
    const bool writeOk = output.good();
    output.close();

    return writeOk && response && response->status == 206 && segmentDownloaded == expected;
}

template <typename Client>
bool DownloadSingleWithResume(Client& client,
                              const std::string& remotePath,
                              const std::filesystem::path& partPath,
                              long long totalSize,
                              const DownloadManager::ProgressCallback& progress,
                              const DownloadManager::SpeedCallback& speedCallback,
                              const DownloadManager::Options& options)
{
    std::error_code error;
    long long offset = 0;
    if (std::filesystem::exists(partPath, error))
        offset = static_cast<long long>(std::filesystem::file_size(partPath, error));

    if (error || offset > totalSize)
    {
        std::filesystem::remove(partPath, error);
        offset = 0;
    }

    const auto overallStart = std::chrono::steady_clock::now();
    std::atomic<long long> downloadedTotal{ offset };

    for (int attempt = 0; attempt <= options.maxRetries; ++attempt)
    {
        client.set_follow_location(true);
        client.set_connection_timeout(options.connectionTimeoutSeconds, 0);

        httplib::Headers headers;
        if (offset > 0)
            headers.emplace("Range", "bytes=" + std::to_string(offset) + "-");

        std::ofstream output(partPath,
            std::ios::binary | (offset > 0 ? std::ios::app : std::ios::trunc));
        if (!output)
            return false;

        long long received = 0;
        const auto response = client.Get(("/Update/" + remotePath).c_str(), headers,
            [&](const char* data, size_t length)
            {
                output.write(data, static_cast<std::streamsize>(length));
                if (!output)
                    return false;

                received += static_cast<long long>(length);
                totalDownloaded.store(offset + received, std::memory_order_relaxed);

                if (progress)
                    progress(totalDownloaded.load(std::memory_order_relaxed), totalSize);

                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - overallStart).count();
                if (speedCallback)
                {
                    const long long current = totalDownloaded.load(std::memory_order_relaxed);
                    speedCallback(elapsed > 0.0 ? current / elapsed : 0.0);
                }

                return true;
            });

        output.flush();
        output.close();

        if (response && ((offset == 0 && response->status == 200) ||
                         (offset > 0 && response->status == 206)))
        {
            offset += received;
            if (offset == totalSize)
                return true;
        }
        else if (offset > 0 && response && response->status == 200)
        {
            std::filesystem::remove(partPath, error);
            offset = 0;
            totalDownloaded.store(0, std::memory_order_relaxed);
        }

        if (attempt < options.maxRetries && options.retryDelayMilliseconds > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(options.retryDelayMilliseconds));
        }

        error.clear();
        if (std::filesystem::exists(partPath, error))
            offset = static_cast<long long>(std::filesystem::file_size(partPath, error));
    }

    return false;
}

bool DownloadMulti(const std::string& host,
                   bool useSsl,
                   const std::string& remotePath,
                   const std::filesystem::path& partPath,
                   const std::filesystem::path& metadataPath,
                   long long totalSize,
                   const DownloadManager::ProgressCallback& progress,
                   const DownloadManager::SpeedCallback& speedCallback,
                   const DownloadManager::Options& options)
{
    if (totalSize <= 0 || options.maxConnections < 2)
        return false;

    const long long segmentSize = std::max(1LL, options.segmentSizeBytes);
    const size_t segmentCount = static_cast<size_t>((totalSize + segmentSize - 1) / segmentSize);

    std::vector<SegmentState> segments;
    if (!LoadMetadata(metadataPath, totalSize, segmentSize, segments))
    {
        segments.reserve(segmentCount);
        for (size_t i = 0; i < segmentCount; ++i)
        {
            SegmentState state;
            state.start = static_cast<long long>(i) * segmentSize;
            state.end = std::min(totalSize - 1, state.start + segmentSize - 1);
            segments.push_back(state);
        }

        std::error_code error;
        std::filesystem::remove(partPath, error);
        if (!PreparePartFile(partPath, totalSize))
            return false;
        if (!SaveMetadata(metadataPath, totalSize, segmentSize, segments))
            return false;
    }
    else
    {
        std::error_code error;
        if (!std::filesystem::exists(partPath, error) ||
            static_cast<long long>(std::filesystem::file_size(partPath, error)) != totalSize)
        {
            std::filesystem::remove(partPath, error);
            if (!PreparePartFile(partPath, totalSize))
                return false;
            for (auto& segment : segments)
                segment.complete = false;
            if (!SaveMetadata(metadataPath, totalSize, segmentSize, segments))
                return false;
        }
    }

    std::atomic<long long> downloadedTotal{ 0 };
    for (const auto& segment : segments)
    {
        if (segment.complete)
            downloadedTotal.fetch_add(segment.end - segment.start + 1, std::memory_order_relaxed);
    }

    std::mutex stateMutex;
    std::mutex callbackMutex;
    const auto overallStart = std::chrono::steady_clock::now();
    std::atomic<size_t> nextSegment{ 0 };
    std::atomic<bool> failed{ false };

    const int connectionCount = std::max(1, std::min(options.maxConnections,
                                                     static_cast<int>(segments.size())));
    std::vector<std::thread> workers;
    workers.reserve(connectionCount);

    auto runSegment = [&](size_t index) -> bool
    {
        auto& segment = segments[index];
        if (segment.complete)
            return true;

        const long long segmentBytes = segment.end - segment.start + 1;
        for (int attempt = 0; attempt <= options.maxRetries; ++attempt)
        {
            std::atomic<long long> segmentProgress{ 0 };
            std::atomic<bool> completed{ false };

            bool success = false;
            auto progressProxy = [&](long long, long long)
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (progress)
                    progress(downloadedTotal.load(std::memory_order_relaxed), totalSize);
            };
            auto speedProxy = [&](double)
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (speedCallback)
                {
                    const double elapsed = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - overallStart).count();
                    const long long current = downloadedTotal.load(std::memory_order_relaxed);
                    speedCallback(elapsed > 0.0 ? current / elapsed : 0.0);
                }
            };

            if (useSsl)
            {
                httplib::SSLClient client(host.c_str());
                client.set_follow_location(true);
                client.set_connection_timeout(options.connectionTimeoutSeconds, 0);
                success = DownloadRange(client, remotePath, partPath,
                                        segment.start, segment.end,
                                        downloadedTotal, totalSize,
                                        progressProxy, speedProxy,
                                        overallStart,
                                        options.connectionTimeoutSeconds);
            }
            else
            {
                httplib::Client client(host.c_str());
                client.set_follow_location(true);
                client.set_connection_timeout(options.connectionTimeoutSeconds, 0);
                success = DownloadRange(client, remotePath, partPath,
                                        segment.start, segment.end,
                                        downloadedTotal, totalSize,
                                        progressProxy, speedProxy,
                                        overallStart,
                                        options.connectionTimeoutSeconds);
            }

            completed.store(success, std::memory_order_relaxed);
            if (completed.load(std::memory_order_relaxed))
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                segment.complete = true;
                if (!SaveMetadata(metadataPath, totalSize, segmentSize, segments))
                    return false;
                return true;
            }

            // DownloadRange may have written only part of the segment. Remove
            // that partial contribution from aggregate progress before retry.
            const long long partial = segmentProgress.load(std::memory_order_relaxed);
            if (partial > 0)
            {
                const long long current = downloadedTotal.load(std::memory_order_relaxed);
                downloadedTotal.store(std::max(0LL, current - partial), std::memory_order_relaxed);
            }

            if (attempt < options.maxRetries && options.retryDelayMilliseconds > 0)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(options.retryDelayMilliseconds));
            }
        }

        return false;
    };

    for (int i = 0; i < connectionCount; ++i)
    {
        workers.emplace_back([&]()
        {
            while (!failed.load(std::memory_order_relaxed))
            {
                const size_t index = nextSegment.fetch_add(1, std::memory_order_relaxed);
                if (index >= segments.size())
                    break;

                if (!runSegment(index))
                {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
            }
        });
    }

    for (auto& worker : workers)
        worker.join();

    if (failed.load(std::memory_order_relaxed))
        return false;

    for (const auto& segment : segments)
    {
        if (!segment.complete)
            return false;
    }

    return true;
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
    if (options_.maxConnections <= 0)
        options_.maxConnections = 1;
    if (options_.segmentSizeBytes <= 0)
        options_.segmentSizeBytes = 4LL * 1024 * 1024;
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
    const auto partPath = MakePartPath(destination);
    const auto metadataPath = MakeMetadataPath(destination);

    RemoteInfo info;
    if (useSsl_)
    {
        httplib::SSLClient client(host_.c_str());
        info = ProbeRemote(client, remotePath, options_.connectionTimeoutSeconds);
    }
    else
    {
        httplib::Client client(host_.c_str());
        info = ProbeRemote(client, remotePath, options_.connectionTimeoutSeconds);
    }

    if (info.size <= 0)
        return false;

    bool success = false;
    if (info.rangeSupported &&
        info.size >= options_.multiConnectionThresholdBytes &&
        options_.maxConnections > 1)
    {
        success = DownloadMulti(host_, useSsl_, remotePath, partPath, metadataPath,
                                info.size, progress, speed, options_);
    }
    else if (info.rangeSupported)
    {
        if (useSsl_)
        {
            httplib::SSLClient client(host_.c_str());
            success = DownloadSingleWithResume(client, remotePath, partPath,
                                               info.size, progress, speed, options_);
        }
        else
        {
            httplib::Client client(host_.c_str());
            success = DownloadSingleWithResume(client, remotePath, partPath,
                                               info.size, progress, speed, options_);
        }
    }
    else
    {
        // Server does not support Range. Keep the staged download path, but
        // do not claim resumability that the server cannot provide.
        std::error_code error;
        std::filesystem::remove(partPath, error);
        std::filesystem::remove(metadataPath, error);

        if (useSsl_)
        {
            httplib::SSLClient client(host_.c_str());
            success = DownloadSingleWithResume(client, remotePath, partPath,
                                               info.size, progress, speed, options_);
        }
        else
        {
            httplib::Client client(host_.c_str());
            success = DownloadSingleWithResume(client, remotePath, partPath,
                                               info.size, progress, speed, options_);
        }
    }

    if (!success)
        return false;

    std::error_code error;
    if (!std::filesystem::exists(partPath, error) ||
        static_cast<long long>(std::filesystem::file_size(partPath, error)) != info.size || error)
    {
        return false;
    }

    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(partPath, destination, error);
    if (error)
        return false;

    std::filesystem::remove(metadataPath, error);
    if (progress)
        progress(info.size, info.size);
    if (speed)
        speed(0.0);

    return true;
}
