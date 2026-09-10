#pragma once

#include <functional>
#include <string>

class DownloadManager
{
public:
    using ProgressCallback = std::function<void(long long, long long)>;
    using SpeedCallback = std::function<void(double)>;

    struct Options
    {
        int maxRetries = 3;
        int connectionTimeoutSeconds = 5;
        int retryDelayMilliseconds = 500;
    };

    DownloadManager(std::string host, bool useSsl, Options options = {});

    std::string Get(const std::string& path) const;
    bool Download(const std::string& remotePath,
                  const std::string& localPath,
                  ProgressCallback progress = {},
                  SpeedCallback speed = {}) const;

private:
    std::string host_;
    bool useSsl_;
    Options options_;
};
