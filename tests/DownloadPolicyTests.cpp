#include <cassert>
#include <iostream>
#include "../Source/NewLauncher/DownloadPolicy.h"

void RunDownloadPolicyTests()
{
    std::cout << "[TEST] Running DownloadPolicy tests..." << std::endl;

    // Small files (< 16 MB) -> 1 connection, 2 MB segments
    {
        auto policy = download_policy::ForSize(1024 * 1024, 1, 4);
        assert(policy.connections == 1);
        assert(policy.segmentSizeBytes == 2LL * 1024 * 1024);
    }

    // Medium files (16 MB - 64 MB) -> 2 connections, 4 MB segments
    {
        auto policy = download_policy::ForSize(32LL * 1024 * 1024, 1, 4);
        assert(policy.connections == 2);
        assert(policy.segmentSizeBytes == 4LL * 1024 * 1024);
    }

    // Large files (64 MB - 256 MB) -> 3 connections, 8 MB segments
    {
        auto policy = download_policy::ForSize(100LL * 1024 * 1024, 1, 4);
        assert(policy.connections == 3);
        assert(policy.segmentSizeBytes == 8LL * 1024 * 1024);
    }

    // Very large files (>= 256 MB) -> max connections (4), 8 MB segments
    {
        auto policy = download_policy::ForSize(500LL * 1024 * 1024, 1, 4);
        assert(policy.connections == 4);
        assert(policy.segmentSizeBytes == 8LL * 1024 * 1024);
    }

    // Respects custom min/max constraints
    {
        auto policy = download_policy::ForSize(500LL * 1024 * 1024, 2, 8);
        assert(policy.connections == 8);

        auto policyRestricted = download_policy::ForSize(500LL * 1024 * 1024, 1, 2);
        assert(policyRestricted.connections == 2);
    }

    std::cout << "[PASS] DownloadPolicy tests passed!" << std::endl;
}
