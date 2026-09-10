#include "UpdateInstaller.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "DownloadPolicy.h"
#include "EndpointManager.h"

UpdateInstaller::UpdateInstaller(std::vector<std::string> hosts,
                                 bool useSsl,
                                 DownloadManager::Options options)
    : hosts_(std::move(hosts))
    , useSsl_(useSsl)
    , options_(options)
{
}

bool UpdateInstaller::Install(const std::vector<Arquivo>& files,
                              int updateCount,
                              ProgressCallback progress,
                              MessageCallback message,
                              SpeedCallback speed) const
{
    if (updateCount <= 0)
    {
        if (progress)
            progress(1.0f, 1.0f);
        return true;
    }

    int index = 1;
    for (const auto& file : files)
    {
        if (!file.ToUpdate)
            continue;

        std::string remotePath = file.FilePath;
        std::replace(remotePath.begin(), remotePath.end(), '\\', '/');

        if (!EnsureDirectory(DirectoryFromPath(file.FilePath)))
            return false;

        if (message)
        {
            const std::string::size_type separator = file.FilePath.find_last_of("/\\");
            message(separator == std::string::npos
                ? file.FilePath
                : file.FilePath.substr(separator + 1));
        }

        if (!DownloadOne(remotePath, file.FilePath, file.FileSize,
                         index, updateCount, progress, speed))
            return false;

        ++index;
    }

    if (progress)
        progress(1.0f, 1.0f);
    return true;
}

bool UpdateInstaller::DownloadOne(const std::string& remotePath,
                                  const std::string& localPath,
                                  long long fileSize,
                                  int fileIndex,
                                  int totalFiles,
                                  const ProgressCallback& progress,
                                  const SpeedCallback& speed) const
{
    DownloadManager::Options effective = options_;
    const auto policy = download_policy::ForSize(
        fileSize,
        1,
        options_.maxConnections);

    if (fileSize > 0)
    {
        effective.maxConnections = policy.connections;
        effective.segmentSizeBytes = policy.segmentSizeBytes;
        effective.multiConnectionThresholdBytes =
            std::min(options_.multiConnectionThresholdBytes,
                     std::max(1LL, fileSize));
    }

    EndpointManager endpoints(hosts_, useSsl_, effective);
    return endpoints.Download(
        remotePath,
        localPath,
        [&](long long downloaded, long long contentLength)
        {
            const float fileProgress = contentLength > 0
                ? std::min(static_cast<float>(downloaded) / static_cast<float>(contentLength), 1.0f)
                : 1.0f;
            const float totalProgress = totalFiles > 0
                ? std::min((static_cast<float>(fileIndex - 1) + fileProgress) /
                           static_cast<float>(totalFiles), 1.0f)
                : 1.0f;

            if (progress)
                progress(fileProgress, totalProgress);
        },
        speed);
}

bool UpdateInstaller::EnsureDirectory(const std::string& directory)
{
    if (directory.empty())
        return true;

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return !error && std::filesystem::is_directory(directory, error) && !error;
}

std::string UpdateInstaller::DirectoryFromPath(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    return separator == std::string::npos ? std::string{} : path.substr(0, separator);
}
