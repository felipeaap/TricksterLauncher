#define NOMINMAX
#include "FileVerifier.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <execution>
#include <string>
#include <utility>

#include "Integrity.h"

namespace
{
std::wstring Utf8ToWide(const std::string& str) noexcept
{
    if (str.empty()) return {};
    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (sizeNeeded <= 0) return {};
    std::wstring wstr(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), wstr.data(), sizeNeeded);
    return wstr;
}

bool GetLocalFileInfo(const std::string& filePath, bool& exists, long long& fileSize) noexcept
{
    const std::wstring widePath = Utf8ToWide(filePath);
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(widePath.c_str(), GetFileExInfoStandard, &data))
    {
        exists = false;
        fileSize = 0;
        return false;
    }

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        exists = false;
        fileSize = 0;
        return false;
    }

    exists = true;
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    fileSize = static_cast<long long>(size.QuadPart);
    return true;
}
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
            bool exists = false;
            long long localSize = 0;
            GetLocalFileInfo(file.FilePath, exists, localSize);

            if (!exists)
            {
                file.ToUpdate = true;
                updates.fetch_add(1, std::memory_order_relaxed);
            }
            else if (file.FileSize > 0 && localSize != file.FileSize)
            {
                // Fast-path: size differs from manifest -> definitely modified, avoid heavy I/O
                file.ToUpdate = true;
                updates.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                // Size matches or is 0: perform full SHA-256 hash verification
                const std::string fileHash = integrity::createHashFromFile(file.FilePath, file.FileHash);
                file.ToUpdate = fileHash.empty() || fileHash != file.FileHash;
                if (file.ToUpdate)
                    updates.fetch_add(1, std::memory_order_relaxed);
            }

            const size_t current = count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress_)
                progress_(current, files.size(), file);
        });

    return updates.load(std::memory_order_relaxed);
}

