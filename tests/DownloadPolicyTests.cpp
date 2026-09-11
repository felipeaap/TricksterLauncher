#include <cassert>
#include <iostream>
#include "../Source/NewLauncher/DownloadPolicy.h"
#include "../Source/NewLauncher/DownloadTelemetry.h"

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

    // ── DownloadTelemetry endpoint throughput and ranking tests ──────────
    {
        download_telemetry::ResetHostMetrics();

        // Host A: Fast (10 MB in 1s = 10 MB/s)
        download_telemetry::Record({ "host-fast.cdn.com", "/file1.pak", 10 * 1024 * 1024, 1.0, 1, false, true });

        // Host B: Slow (1 MB in 2s = 0.5 MB/s)
        download_telemetry::Record({ "host-slow.cdn.com", "/file1.pak", 1 * 1024 * 1024, 2.0, 1, false, true });

        // Host C: Broken (failing multiple times)
        download_telemetry::Record({ "host-broken.cdn.com", "/file1.pak", 0, 0.5, 1, false, false });
        download_telemetry::Record({ "host-broken.cdn.com", "/file1.pak", 0, 0.5, 1, false, false });
        download_telemetry::Record({ "host-broken.cdn.com", "/file1.pak", 0, 0.5, 1, false, false });

        std::vector<std::string> candidates = { "host-slow.cdn.com", "host-broken.cdn.com", "host-fast.cdn.com" };
        auto ranked = download_telemetry::GetRankedHosts(candidates);

        assert(ranked.size() == 3);
        assert(ranked[0] == "host-fast.cdn.com" && "Fastest endpoint should be ranked first");
        assert(ranked[1] == "host-slow.cdn.com" && "Slow endpoint should be ranked before broken");
        assert(ranked[2] == "host-broken.cdn.com" && "Broken endpoint with consecutive failures must be ranked last");

        assert(download_telemetry::GetHostThroughput("host-fast.cdn.com") > download_telemetry::GetHostThroughput("host-slow.cdn.com"));
    }

    std::cout << "[PASS] DownloadPolicy and Telemetry tests passed!" << std::endl;
}
