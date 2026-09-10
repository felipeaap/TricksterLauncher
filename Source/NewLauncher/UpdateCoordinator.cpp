#include "UpdateCoordinator.h"

#include <algorithm>

#include "FileVerifier.h"
#include "ManifestManager.h"

UpdateCoordinator::UpdateCoordinator(FetchFunction fetch,
                                     MessageCallback message,
                                     ProgressCallback progress)
    : fetch_(std::move(fetch))
    , message_(std::move(message))
    , progress_(std::move(progress))
{
}

void UpdateCoordinator::Check(std::vector<Arquivo>& files,
                              bool fullCheck,
                              int& localVersion,
                              int& currentVersion,
                              int& updateCount)
{
    ManifestManager manifests(fetch_);
    currentVersion = manifests.Load(files, fullCheck, localVersion);

    FileVerifier verifier([this](size_t current, size_t total, const Arquivo& file)
    {
        const float percent = total > 0
            ? std::min(static_cast<float>(current) / static_cast<float>(total), 1.0f)
            : 1.0f;

        const std::string fileName = file.FilePath.substr(file.FilePath.find_last_of("/\\") + 1);
        if (progress_)
            progress_(percent, percent);
        if (message_)
            message_(fileName);
    });

    updateCount = verifier.CountUpdates(files);
}
