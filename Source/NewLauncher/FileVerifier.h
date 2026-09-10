#pragma once

#include <functional>
#include <string>
#include <vector>

#include "UpdateTypes.h"

class FileVerifier
{
public:
    using ProgressCallback = std::function<void(size_t, size_t, const Arquivo&)>;

    explicit FileVerifier(ProgressCallback progress = {});

    int CountUpdates(std::vector<Arquivo>& files) const;

private:
    ProgressCallback progress_;
};
