#pragma once

#include <string>

struct DownloadTelemetryRecord
{
    std::string host;
    std::string path;
    long long bytes = 0;
    double seconds = 0.0;
    int connections = 1;
    bool ranged = false;
    bool success = false;
};

namespace download_telemetry
{
    void Record(const DownloadTelemetryRecord& record) noexcept;
}
