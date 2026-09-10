#include "DownloadPolicy.h"

#include <algorithm>

namespace download_policy
{
DownloadPolicy ForSize(long long bytes, int minConnections, int maxConnections) noexcept
{
    const int minimum = std::max(1, minConnections);
    const int maximum = std::max(minimum, maxConnections);

    DownloadPolicy policy;
    if (bytes >= 256LL * 1024 * 1024)
    {
        policy.connections = maximum;
        policy.segmentSizeBytes = 8LL * 1024 * 1024;
    }
    else if (bytes >= 64LL * 1024 * 1024)
    {
        policy.connections = std::min(maximum, 3);
        policy.segmentSizeBytes = 8LL * 1024 * 1024;
    }
    else if (bytes >= 16LL * 1024 * 1024)
    {
        policy.connections = std::min(maximum, 2);
        policy.segmentSizeBytes = 4LL * 1024 * 1024;
    }
    else
    {
        policy.connections = minimum;
        policy.segmentSizeBytes = 2LL * 1024 * 1024;
    }

    policy.connections = std::clamp(policy.connections, minimum, maximum);
    return policy;
}
}
