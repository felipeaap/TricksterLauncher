#include "DownloadManager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "DownloadTelemetry.h"

namespace
{
std::filesystem::path MakeDownloadsStagingPath(const std::filesystem::path& destination)
{
    const std::filesystem::path downloadsBase = "downloads";

    std::filesystem::path rel = destination.lexically_normal();
    if (destination.is_absolute())
    {
        std::error_code ec;
        auto relative = std::filesystem::relative(destination, std::filesystem::current_path(), ec);
        if (!ec && !relative.empty() && relative.native().find(L"..") == std::wstring::npos)
        {
            rel = relative.lexically_normal();
        }
        else
        {
            rel = destination.filename();
        }
    }

    std::filesystem::path cleanRel;
    for (const auto& part : rel)
    {
        if (part == "." || part == "..")
            continue;
        cleanRel /= part;
    }

    if (cleanRel.empty())
        cleanRel = destination.filename();

    return downloadsBase / cleanRel;
}

void CleanTemporaryDownloadFiles(const std::filesystem::path& destination)
{
    std::error_code ec;
    const std::filesystem::path staged = MakeDownloadsStagingPath(destination);
    const std::filesystem::path partPath   = staged.string() + ".part";
    const std::filesystem::path metaPath   = staged.string() + ".part.meta";
    const std::filesystem::path tmpPath    = metaPath.string() + ".tmp";
    const std::filesystem::path bakPath    = staged.string() + ".bak";
    const std::filesystem::path backupPath = staged.string() + ".backup";

    std::filesystem::remove(partPath, ec);   ec.clear();
    std::filesystem::remove(metaPath, ec);   ec.clear();
    std::filesystem::remove(tmpPath, ec);    ec.clear();
    std::filesystem::remove(bakPath, ec);    ec.clear();
    std::filesystem::remove(backupPath, ec); ec.clear();

    std::filesystem::remove(destination.string() + ".part", ec); ec.clear();
    std::filesystem::remove(destination.string() + ".part.meta", ec); ec.clear();
    std::filesystem::remove(destination.string() + ".bak", ec); ec.clear();
    std::filesystem::remove(destination.string() + ".backup", ec); ec.clear();
}

struct RemoteInfo
{
    long long size = -1;
    bool rangeSupported = false;
    bool exists = false;
    int status = 0;
};

struct SegmentState
{
    long long start = 0;
    long long end = 0;
    bool complete = false;
};

template <typename Client>
void ConfigureClient(Client& client, int timeoutSeconds)
{
    client.set_follow_location(true);
    client.set_keep_alive(true);
    client.set_tcp_nodelay(true);
    client.set_connection_timeout(timeoutSeconds, 0);
    client.set_read_timeout(timeoutSeconds, 0);
    client.set_write_timeout(timeoutSeconds, 0);
    client.set_socket_options([](socket_t sock)
    {
        int bufSize = 1024 * 1024; // 1 MB TCP receive buffer for high-latency BDP links
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    });
}

long long CalculateBackoffWithJitter(int baseDelayMs, int attempt)
{
    if (baseDelayMs <= 0)
        return 0;

    const long long expBackoff = static_cast<long long>(baseDelayMs) * (1LL << (std::min)(attempt, 4));
    thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<long long> dist(expBackoff / 2, expBackoff);
    return dist(rng);
}

template <typename Client>
bool FetchImpl(Client& client, const std::string& path, std::string& outBody, int timeoutSeconds)
{
    ConfigureClient(client, timeoutSeconds);

    const auto response = client.Get(path.c_str());
    if (!response || response->status != 200)
    {
        outBody.clear();
        return false;
    }

    outBody = response->body;
    return true;
}

template <typename Client>
std::string GetImpl(Client& client, const std::string& path, int timeoutSeconds)
{
    std::string body;
    FetchImpl(client, path, body, timeoutSeconds);
    return body;
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

    return total >= 0;
}

bool ParseDetailedContentRange(const std::string& value,
                               long long& rangeStart,
                               long long& rangeEnd,
                               long long& total)
{
    const auto bytesPos = value.find("bytes ");
    const auto hyphenPos = value.find('-');
    const auto slashPos = value.find('/');

    if (bytesPos == std::string::npos || hyphenPos == std::string::npos || slashPos == std::string::npos)
        return false;

    try
    {
        rangeStart = std::stoll(value.substr(bytesPos + 6, hyphenPos - (bytesPos + 6)));
        rangeEnd = std::stoll(value.substr(hyphenPos + 1, slashPos - (hyphenPos + 1)));
        const std::string totalStr = value.substr(slashPos + 1);
        total = (totalStr == "*") ? -1 : std::stoll(totalStr);
    }
    catch (...)
    {
        return false;
    }

    return rangeStart >= 0 && rangeEnd >= rangeStart;
}

template <typename Client>
RemoteInfo ProbeRemote(Client& client,
                       const std::string& remotePath,
                       int timeoutSeconds)
{
    RemoteInfo info;
    ConfigureClient(client, timeoutSeconds);

    const std::string& url = remotePath;
    const auto head = client.Head(url.c_str());
    if (head)
    {
        info.status = head->status;
        if (head->status == 200 || head->status == 206)
        {
            info.exists = true;
            const auto length = head->headers.find("Content-Length");
            if (length != head->headers.end())
            {
                try
                {
                    info.size = std::stoll(length->second);
                }
                catch (...)
                {
                    info.size = -1;
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
    }

    // A number of CDNs omit Accept-Ranges on HEAD or do not support HEAD.
    // Probe one byte while discarding body to check Range and get content length.
    if (!info.exists || !info.rangeSupported)
    {
        httplib::Headers headers;
        headers.emplace("Range", "bytes=0-0");

        const auto response = client.Get(url.c_str(), headers,
            [](const char*, size_t)
            {
                return false;
            });

        if (response)
        {
            info.status = response->status;
            if (response->status == 206)
            {
                info.exists = true;
                const auto range = response->headers.find("Content-Range");
                if (range != response->headers.end())
                    info.rangeSupported = ParseContentRange(range->second, info.size);
            }
            else if (response->status == 200)
            {
                info.exists = true;
                const auto length = response->headers.find("Content-Length");
                if (length != response->headers.end())
                {
                    try { info.size = std::stoll(length->second); } catch (...) { info.size = -1; }
                }
            }
        }
    }

    return info;
}

std::filesystem::path MakePartPath(const std::filesystem::path& destination)
{
    auto staged = MakeDownloadsStagingPath(destination);
    staged += ".part";
    return staged;
}

std::filesystem::path MakeMetadataPath(const std::filesystem::path& destination)
{
    auto staged = MakeDownloadsStagingPath(destination);
    staged += ".part.meta";
    return staged;
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
                  const std::string& expectedHash,
                  std::vector<SegmentState>& segments)
{
    std::ifstream input(metadataPath);
    if (!input)
        return false;

    long long storedSize = 0;
    long long storedSegmentSize = 0;
    std::string storedHash;
    size_t count = 0;
    if (!(input >> storedSize >> storedSegmentSize >> storedHash >> count))
        return false;

    if (storedSize != remoteSize || storedSegmentSize != segmentSize || count == 0)
        return false;

    if (!expectedHash.empty() && storedHash != expectedHash && storedHash != "*")
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
        state.end = (std::min)(remoteSize - 1, state.start + segmentSize - 1);
        state.complete = complete != 0;
        segments.push_back(state);
    }

    return true;
}

bool SaveMetadata(const std::filesystem::path& metadataPath,
                  long long remoteSize,
                  long long segmentSize,
                  const std::string& expectedHash,
                  const std::vector<SegmentState>& segments)
{
    std::error_code error;
    if (metadataPath.has_parent_path())
    {
        std::filesystem::create_directories(metadataPath.parent_path(), error);
        error.clear();
    }

    const auto temporaryPath = metadataPath.string() + ".tmp";
    std::ofstream output(temporaryPath, std::ios::trunc);
    if (!output)
        return false;

    const std::string hashToStore = expectedHash.empty() ? "*" : expectedHash;
    output << remoteSize << ' ' << segmentSize << ' ' << hashToStore << ' ' << segments.size() << '\n';
    for (const auto& segment : segments)
        output << (segment.complete ? 1 : 0) << '\n';

    output.flush();
    output.close();
    if (!output)
        return false;

    error.clear();
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
                   std::atomic<long long>& segmentProgress,
                   const DownloadManager::ProgressCallback& progress,
                   const DownloadManager::SpeedCallback& speedCallback,
                   const std::chrono::steady_clock::time_point& overallStart,
                   int timeoutSeconds)
{
    if (start > end)
        return true;

    ConfigureClient(client, timeoutSeconds);

    httplib::Headers headers;
    headers.emplace("Range", "bytes=" + std::to_string(start) + "-" + std::to_string(end));

    std::fstream output(partPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!output)
        return false;

    output.seekp(start);
    if (!output)
        return false;

    long long received = 0;
    const long long expected = end - start + 1;

    const auto response = client.Get(remotePath.c_str(), headers,
        [&](const char* data, size_t length)
        {
            if (received + static_cast<long long>(length) > expected)
                return false;

            output.write(data, static_cast<std::streamsize>(length));
            if (!output)
                return false;

            received += static_cast<long long>(length);
            segmentProgress.store(received, std::memory_order_relaxed);

            if (progress)
                progress(received, expected);

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - overallStart).count();
            if (speedCallback)
                speedCallback(elapsed > 0.0 ? received / elapsed : 0.0);

            return true;
        });

    output.flush();
    const bool writeOk = output.good();
    output.close();

    bool rangeHeaderValid = false;
    if (response && response->status == 206)
    {
        const auto it = response->headers.find("Content-Range");
        if (it != response->headers.end())
        {
            long long rStart = 0, rEnd = 0, rTotal = 0;
            if (ParseDetailedContentRange(it->second, rStart, rEnd, rTotal))
            {
                rangeHeaderValid = (rStart == start);
            }
        }
        else
        {
            rangeHeaderValid = (received == expected);
        }
    }

    return writeOk && response && response->status == 206 && rangeHeaderValid && (received == expected);
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
    if (partPath.has_parent_path())
    {
        std::filesystem::create_directories(partPath.parent_path(), error);
        error.clear();
    }

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
        ConfigureClient(client, options.connectionTimeoutSeconds);

        httplib::Headers headers;
        if (offset > 0)
            headers.emplace("Range", "bytes=" + std::to_string(offset) + "-");

        std::ofstream output(partPath,
            std::ios::binary | (offset > 0 ? std::ios::app : std::ios::trunc));
        if (!output)
            return false;

        long long received = 0;
        const auto response = client.Get(remotePath.c_str(), headers,
            [&](const char* data, size_t length)
            {
                output.write(data, static_cast<std::streamsize>(length));
                if (!output)
                    return false;

                received += static_cast<long long>(length);
                downloadedTotal.store(offset + received, std::memory_order_relaxed);

                if (progress)
                    progress(downloadedTotal.load(std::memory_order_relaxed), totalSize);

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
        output.close();

        if (response && response->status == 206 && offset > 0)
        {
            // Strict check of Content-Range
            bool validRange = true;
            const auto it = response->headers.find("Content-Range");
            if (it != response->headers.end())
            {
                long long rStart = 0, rEnd = 0, rTotal = 0;
                if (ParseDetailedContentRange(it->second, rStart, rEnd, rTotal))
                {
                    if (rStart != offset)
                        validRange = false;
                }
            }

            if (!validRange)
            {
                std::filesystem::remove(partPath, error);
                offset = 0;
                downloadedTotal.store(0, std::memory_order_relaxed);
            }
            else
            {
                offset += received;
                if (offset == totalSize)
                    return true;
            }
        }
        else if (response && response->status == 200)
        {
            if (offset > 0)
            {
                // Server sent entire file from 0 instead of partial range!
                // Since output was in append mode, reset to 0 and restart cleanly.
                std::filesystem::remove(partPath, error);
                offset = 0;
                downloadedTotal.store(0, std::memory_order_relaxed);
            }
            else
            {
                offset += received;
                if (offset == totalSize)
                    return true;
            }
        }
        else if (response && response->status == 416)
        {
            std::filesystem::remove(partPath, error);
            offset = 0;
            downloadedTotal.store(0, std::memory_order_relaxed);
        }

        if (attempt < options.maxRetries && options.retryDelayMilliseconds > 0)
        {
            const long long backoffMs = CalculateBackoffWithJitter(options.retryDelayMilliseconds, attempt);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
        }

        error.clear();
        if (std::filesystem::exists(partPath, error))
            offset = static_cast<long long>(std::filesystem::file_size(partPath, error));
    }

    return false;
}

void ParseHostAndPath(const std::string& rawHost, bool useSsl, std::string& outHost, int& outPort, std::string& outBasePath)
{
    std::string s = rawHost;
    if (s.rfind("https://", 0) == 0)
        s = s.substr(8);
    else if (s.rfind("http://", 0) == 0)
        s = s.substr(7);

    const auto slashPos = s.find('/');
    std::string hostPart;
    if (slashPos != std::string::npos)
    {
        hostPart = s.substr(0, slashPos);
        outBasePath = s.substr(slashPos);
    }
    else
    {
        hostPart = s;
        outBasePath.clear();
    }

    if (!outBasePath.empty() && outBasePath.front() != '/')
        outBasePath = '/' + outBasePath;

    while (outBasePath.size() > 1 && outBasePath.back() == '/')
        outBasePath.pop_back();

    outPort = useSsl ? 443 : 80;
    const auto colonPos = hostPart.rfind(':');
    if (colonPos != std::string::npos)
    {
        outHost = hostPart.substr(0, colonPos);
        try { outPort = std::stoi(hostPart.substr(colonPos + 1)); }
        catch (...) {}
    }
    else
    {
        outHost = hostPart;
    }
}

bool DownloadMulti(const std::string& host,
                   int port,
                   bool useSsl,
                   const std::string& remotePath,
                   const std::filesystem::path& partPath,
                   const std::filesystem::path& metadataPath,
                   long long totalSize,
                   const DownloadManager::ProgressCallback& progress,
                   const DownloadManager::SpeedCallback& speedCallback,
                   const DownloadManager::ErrorCallback& errorCallback,
                   const std::string& expectedHash,
                   const DownloadManager::Options& options)
{
    if (totalSize <= 0 || options.maxConnections < 2)
        return false;

    const long long segmentSize = (std::max)(1LL, options.segmentSizeBytes);
    const size_t segmentCount = static_cast<size_t>((totalSize + segmentSize - 1) / segmentSize);

    std::vector<SegmentState> segments;
    if (!LoadMetadata(metadataPath, totalSize, segmentSize, expectedHash, segments))
    {
        segments.reserve(segmentCount);
        for (size_t i = 0; i < segmentCount; ++i)
        {
            SegmentState state;
            state.start = static_cast<long long>(i) * segmentSize;
            state.end = (std::min)(totalSize - 1, state.start + segmentSize - 1);
            segments.push_back(state);
        }

        std::error_code error;
        std::filesystem::remove(partPath, error);
        if (!PreparePartFile(partPath, totalSize))
            return false;
        if (!SaveMetadata(metadataPath, totalSize, segmentSize, expectedHash, segments))
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
            if (!SaveMetadata(metadataPath, totalSize, segmentSize, expectedHash, segments))
                return false;
        }
    }

    std::vector<std::atomic<long long>> segmentProgresses(segments.size());
    for (size_t i = 0; i < segments.size(); ++i)
    {
        const long long bytes = segments[i].end - segments[i].start + 1;
        segmentProgresses[i].store(segments[i].complete ? bytes : 0, std::memory_order_relaxed);
    }

    std::mutex stateMutex;
    std::mutex callbackMutex;
    const auto overallStart = std::chrono::steady_clock::now();
    std::atomic<size_t> nextSegment{ 0 };
    std::atomic<bool> failed{ false };

    const auto aggregateProgress = [&]() -> long long
    {
        long long total = 0;
        for (auto& segmentProgress : segmentProgresses)
            total += segmentProgress.load(std::memory_order_relaxed);
        return (std::min)(total, totalSize);
    };

    const int connectionCount = (std::max)(1, (std::min)(options.maxConnections,
                                                         static_cast<int>(segments.size())));
    std::vector<std::thread> workers;
    workers.reserve(connectionCount);

    auto runSegment = [&](size_t index) -> bool
    {
        auto& segment = segments[index];
        if (segment.complete)
            return true;

        for (int attempt = 0; attempt <= options.maxRetries; ++attempt)
        {
            segmentProgresses[index].store(0, std::memory_order_relaxed);

            bool success = false;
            auto progressProxy = [&](long long, long long)
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (progress)
                    progress(aggregateProgress(), totalSize);
            };
            auto speedProxy = [&](double)
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (speedCallback)
                {
                    const double elapsed = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - overallStart).count();
                    const long long current = aggregateProgress();
                    speedCallback(elapsed > 0.0 ? current / elapsed : 0.0);
                }
            };

            if (useSsl)
            {
                httplib::SSLClient client(host, port);
                success = DownloadRange(client, remotePath, partPath,
                                        segment.start, segment.end,
                                        segmentProgresses[index],
                                        progressProxy, speedProxy,
                                        overallStart,
                                        options.connectionTimeoutSeconds);
            }
            else
            {
                httplib::Client client(host, port);
                success = DownloadRange(client, remotePath, partPath,
                                        segment.start, segment.end,
                                        segmentProgresses[index],
                                        progressProxy, speedProxy,
                                        overallStart,
                                        options.connectionTimeoutSeconds);
            }

            if (success)
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                segment.complete = true;
                segmentProgresses[index].store(
                    segment.end - segment.start + 1, std::memory_order_relaxed);

                if (!SaveMetadata(metadataPath, totalSize, segmentSize, expectedHash, segments))
                    return false;
                return true;
            }

            segmentProgresses[index].store(0, std::memory_order_relaxed);

            if (attempt < options.maxRetries && options.retryDelayMilliseconds > 0)
            {
                const long long backoffMs = CalculateBackoffWithJitter(options.retryDelayMilliseconds, attempt);
                std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
            }
        }

        if (errorCallback)
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            errorCallback("Segment " + std::to_string(index) + " failed after retries: " + remotePath);
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
    ParseHostAndPath(host_, useSsl_, cleanHost_, port_, basePath_);

    if (options_.maxRetries < 0)
        options_.maxRetries = 0;
    if (options_.connectionTimeoutSeconds <= 0)
        options_.connectionTimeoutSeconds = 10;
    if (options_.retryDelayMilliseconds < 0)
        options_.retryDelayMilliseconds = 0;
    if (options_.maxConnections <= 0)
        options_.maxConnections = 1;
    if (options_.segmentSizeBytes <= 0)
        options_.segmentSizeBytes = 4LL * 1024 * 1024;
}

std::string DownloadManager::CombinePath(const std::string& path) const
{
    if (basePath_.empty())
    {
        if (path.empty() || path.front() != '/')
            return "/" + path;
        return path;
    }

    std::string result = basePath_;
    if (result.back() == '/' && !path.empty() && path.front() == '/')
    {
        result.pop_back();
    }
    else if (result.back() != '/' && (path.empty() || path.front() != '/'))
    {
        result += '/';
    }
    result += path;
    return result;
}

bool DownloadManager::Fetch(const std::string& path, std::string& outBody) const
{
    const std::string fullPath = CombinePath(path);
    if (useSsl_)
    {
        httplib::SSLClient client(cleanHost_, port_);
        return FetchImpl(client, fullPath, outBody, options_.connectionTimeoutSeconds);
    }

    httplib::Client client(cleanHost_, port_);
    return FetchImpl(client, fullPath, outBody, options_.connectionTimeoutSeconds);
}

std::string DownloadManager::Get(const std::string& path) const
{
    std::string body;
    Fetch(path, body);
    return body;
}

bool DownloadManager::Download(const std::string& remotePath,
                               const std::string& localPath,
                               ProgressCallback progress,
                               SpeedCallback speed,
                               ErrorCallback errorCallback,
                               const std::string& expectedHash) const
{
    const auto overallStartTime = std::chrono::steady_clock::now();
    const std::filesystem::path destination(localPath);
    const auto partPath = MakePartPath(destination);
    const auto metadataPath = MakeMetadataPath(destination);
    const std::string fullRemotePath = CombinePath(remotePath);

    RemoteInfo info;
    if (useSsl_)
    {
        httplib::SSLClient client(cleanHost_, port_);
        info = ProbeRemote(client, fullRemotePath, options_.connectionTimeoutSeconds);
    }
    else
    {
        httplib::Client client(cleanHost_, port_);
        info = ProbeRemote(client, fullRemotePath, options_.connectionTimeoutSeconds);
    }

    // 1. Handle valid 0-byte (empty) files
    if (info.exists && info.size == 0)
    {
        std::error_code error;
        std::filesystem::remove(partPath, error);
        std::filesystem::remove(metadataPath, error);
        std::filesystem::remove(destination, error);

        std::ofstream emptyFile(destination, std::ios::binary | std::ios::trunc);
        emptyFile.close();

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - overallStartTime).count();
        download_telemetry::Record({ host_, remotePath, 0, elapsed, 1, false, true });

        if (progress)
            progress(0, 0);
        if (speed)
            speed(0.0);
        return true;
    }

    bool success = false;
    const bool isMulti = (info.exists && info.rangeSupported &&
                          info.size >= options_.multiConnectionThresholdBytes &&
                          options_.maxConnections > 1);

    if (isMulti)
    {
        success = DownloadMulti(cleanHost_, port_, useSsl_, fullRemotePath, partPath, metadataPath,
                                info.size, progress, speed, errorCallback, expectedHash, options_);
    }
    else
    {
        // Standard single download with resume / fallback
        std::error_code error;
        if (!info.rangeSupported)
        {
            std::filesystem::remove(partPath, error);
            std::filesystem::remove(metadataPath, error);
        }

        const long long targetSize = (info.size > 0) ? info.size : 0;
        if (useSsl_)
        {
            httplib::SSLClient client(cleanHost_, port_);
            success = DownloadSingleWithResume(client, fullRemotePath, partPath,
                                               targetSize, progress, speed, options_);
        }
        else
        {
            httplib::Client client(cleanHost_, port_);
            success = DownloadSingleWithResume(client, fullRemotePath, partPath,
                                               targetSize, progress, speed, options_);
        }
    }

    if (!success)
    {
        CleanTemporaryDownloadFiles(destination);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - overallStartTime).count();
        download_telemetry::Record({ host_, remotePath, 0, elapsed, isMulti ? options_.maxConnections : 1, isMulti, false });
        if (errorCallback)
            errorCallback("Download failed for: " + remotePath);
        return false;
    }

    std::error_code error;
    if (!std::filesystem::exists(partPath, error))
    {
        CleanTemporaryDownloadFiles(destination);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - overallStartTime).count();
        download_telemetry::Record({ host_, remotePath, 0, elapsed, isMulti ? options_.maxConnections : 1, isMulti, false });
        if (errorCallback)
            errorCallback("Downloaded file missing part file for: " + remotePath);
        return false;
    }

    const auto downloadedBytes = static_cast<long long>(std::filesystem::file_size(partPath, error));
    if (info.size > 0 && downloadedBytes != info.size)
    {
        CleanTemporaryDownloadFiles(destination);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - overallStartTime).count();
        download_telemetry::Record({ host_, remotePath, 0, elapsed, isMulti ? options_.maxConnections : 1, isMulti, false });
        if (errorCallback)
            errorCallback("Downloaded file size mismatch for: " + remotePath);
        return false;
    }

    if (destination.has_parent_path())
    {
        std::filesystem::create_directories(destination.parent_path(), error);
        error.clear();
    }

    if (std::filesystem::exists(destination, error))
    {
        SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_NORMAL);
        std::filesystem::remove(destination, error);
        error.clear();
    }

    std::filesystem::rename(partPath, destination, error);
    if (error)
    {
        error.clear();
        std::filesystem::copy_file(
            partPath,
            destination,
            std::filesystem::copy_options::overwrite_existing,
            error);

        if (error)
        {
            CleanTemporaryDownloadFiles(destination);
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - overallStartTime).count();
            download_telemetry::Record({ host_, remotePath, 0, elapsed, isMulti ? options_.maxConnections : 1, isMulti, false });
            if (errorCallback)
                errorCallback("Failed to move completed part file to final destination: " + destination.string());
            return false;
        }
        std::filesystem::remove(partPath, error);
    }

    CleanTemporaryDownloadFiles(destination);

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - overallStartTime).count();
    download_telemetry::Record({ host_, remotePath, downloadedBytes, elapsed, isMulti ? options_.maxConnections : 1, isMulti, true });

    if (progress)
        progress(downloadedBytes, downloadedBytes);
    if (speed)
        speed(0.0);

    return true;
}


