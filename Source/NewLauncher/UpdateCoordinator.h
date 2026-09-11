#pragma once

#include <functional>
#include <thread>
#include <vector>

#include "UpdateTypes.h"

class UpdateCoordinator
{
public:
    using FetchFunction = std::function<std::string(const std::string&)>;
    using MessageCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(float, float)>;

    UpdateCoordinator(FetchFunction fetch,
                      MessageCallback message,
                      ProgressCallback progress);

    void Check(std::vector<Arquivo>& files, bool fullCheck, int& localVersion, int& currentVersion, int& updateCount);

private:
    FetchFunction fetch_;
    MessageCallback message_;
    ProgressCallback progress_;
};
