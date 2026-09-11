#include "FileVerifier.h"

#include <algorithm>
#include <atomic>
#include <execution>
#include <fstream>
#include <semaphore>
#include <string>
#include <utility>

#include "Integrity.h"

namespace
{
constexpr int kMaxConcurrent = 8;
std::counting_semaphore<kMaxConcurrent> g_fileSemaphore(kMaxConcurrent);
}

FileVerifier::FileVerifier(ProgressCallback progress)
    : progress_(std::move(progress))
{
}

int FileVerifier::CountUpdates(std::vector<Arquivo>& files) const
{
    if (files.empty())
        return 0;

    std::atomic<int> updates{ 0 };
    std::atomic<size_t> count{ 0 };

    std::for_each(std::execution::par, files.begin(), files.end(),
        [&](Arquivo& file)
        {
            g_fileSemaphore.acquire();

            const bool exists = std::ifstream(file.FilePath).good();
            if (!exists)
            {
                file.ToUpdate = true;
                updates.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                const std::string fileHash = integrity::createHashFromFile(file.FilePath, file.FileHash);
                file.ToUpdate = fileHash.empty() || fileHash != file.FileHash;
                if (file.ToUpdate)
                    updates.fetch_add(1, std::memory_order_relaxed);
            }

            const size_t current = count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress_)
                progress_(current, files.size(), file);

            g_fileSemaphore.release();
        });

    return updates.load(std::memory_order_relaxed);
}
