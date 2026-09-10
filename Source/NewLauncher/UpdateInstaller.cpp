#include "UpdateInstaller.h"

#include <algorithm>
#include <direct.h>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>

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

        if (!DownloadOne(remotePath, file.FilePath, index, updateCount, progress, speed))
            return false;

        ++index;
    }

    if (progress)
        progress(1.0f, 1.0f);
    return true;
}

bool UpdateInstaller::DownloadOne(const std::string& remotePath,
                                  const std::string& localPath,
                                  int fileIndex,
                                  int totalFiles,
                                  const ProgressCallback& progress,
                                  const SpeedCallback& speed) const
{
    EndpointManager endpoints(hosts_, useSsl_, options_);
    const bool result = endpoints.Download(
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

    return result;
}

bool UpdateInstaller::EnsureDirectory(const std::string& directory)
{
    if (directory.empty())
        return true;

    std::string fixedPath = directory;
    std::replace(fixedPath.begin(), fixedPath.end(), '\\', '/');

    std::string current;
    if (fixedPath.size() >= 2 && fixedPath[1] == ':')
        current = fixedPath.substr(0, 2);

    std::stringstream stream(fixedPath);
    std::string token;
    while (std::getline(stream, token, '/'))
    {
        if (token.empty())
            continue;

        if (!current.empty() && current.back() != ':')
            current += '/';
        current += token;

        if (_mkdir(current.c_str()) != 0)
        {
            // A pre-existing directory is the expected case.
            // Other errors are checked by the caller when the file is opened.
        }
    }

    return true;
}

std::string UpdateInstaller::DirectoryFromPath(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    return separator == std::string::npos ? std::string{} : path.substr(0, separator);
}
