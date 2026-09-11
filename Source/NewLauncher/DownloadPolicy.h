#pragma once

struct DownloadPolicy
{
    int connections = 1;
    long long segmentSizeBytes = 4LL * 1024 * 1024;
};

namespace download_policy
{
    DownloadPolicy ForSize(long long bytes,
                           int minConnections = 1,
                           int maxConnections = 4) noexcept;
}
