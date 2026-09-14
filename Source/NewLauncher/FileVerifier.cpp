#define NOMINMAX
#include "FileVerifier.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <execution>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "Integrity.h"
#include "json.hpp"

namespace
{
struct CacheEntry
{
    long long fileSize = 0;
    unsigned long long lastWriteTime = 0;
    std::string hash;
};

std::wstring Utf8ToWide(const std::string& str) noexcept
{
    if (str.empty()) return {};
    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (sizeNeeded <= 0) return {};
    std::wstring wstr(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), wstr.data(), sizeNeeded);
    return wstr;
}

bool GetLocalFileInfo(const std::string& filePath, bool& exists, long long& fileSize, unsigned long long& lastWriteTime) noexcept
{
    const std::wstring widePath = Utf8ToWide(filePath);
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(widePath.c_str(), GetFileExInfoStandard, &data))
    {
        exists = false;
        fileSize = 0;
        lastWriteTime = 0;
        return false;
    }

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        exists = false;
        fileSize = 0;
        lastWriteTime = 0;
        return false;
    }

    exists = true;
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    fileSize = static_cast<long long>(size.QuadPart);

    ULARGE_INTEGER ft{};
    ft.HighPart = data.ftLastWriteTime.dwHighDateTime;
    ft.LowPart = data.ftLastWriteTime.dwLowDateTime;
    lastWriteTime = ft.QuadPart;
    return true;
}

const std::filesystem::path kCachePath = "LauncherData/.hash_cache.json";

std::unordered_map<std::string, CacheEntry> LoadHashCache() noexcept
{
    std::unordered_map<std::string, CacheEntry> cache;
    try
    {
        std::ifstream file(kCachePath);
        if (!file.is_open()) return cache;

        nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
        if (!j.is_object()) return cache;

        for (auto& [key, item] : j.items())
        {
            if (!item.is_object()) continue;
            CacheEntry entry;
            entry.fileSize = item.value("size", 0LL);
            entry.lastWriteTime = item.value("mtime", 0ULL);
            entry.hash = item.value("hash", "");
            if (!entry.hash.empty())
            {
                cache.emplace(key, std::move(entry));
            }
        }
    }
    catch (...) {}
    return cache;
}

void SaveHashCache(const std::unordered_map<std::string, CacheEntry>& cache) noexcept
{
    try
    {
        std::filesystem::create_directories(kCachePath.parent_path());
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [key, entry] : cache)
        {
            j[key] = {
                { "size", entry.fileSize },
                { "mtime", entry.lastWriteTime },
                { "hash", entry.hash }
            };
        }

        std::ofstream file(kCachePath);
        if (file.is_open())
        {
            file << j.dump();
        }
    }
    catch (...) {}
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

    auto cache = LoadHashCache();
    std::mutex cacheMutex;
    std::atomic<bool> cacheModified{ false };

    std::for_each(std::execution::par, files.begin(), files.end(),
        [&](Arquivo& file)
        {
            bool exists = false;
            long long localSize = 0;
            unsigned long long lastWriteTime = 0;
            GetLocalFileInfo(file.FilePath, exists, localSize, lastWriteTime);

            if (!exists)
            {
                file.ToUpdate = true;
                updates.fetch_add(1, std::memory_order_relaxed);
            }
            else if (file.FileSize > 0 && localSize != file.FileSize)
            {
                // Fast-path 1: size differs from manifest -> definitely modified, avoid heavy I/O
                file.ToUpdate = true;
                updates.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                // Fast-path 2: Check timestamp cache
                std::string fileHash;
                bool foundInCache = false;
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    auto it = cache.find(file.FilePath);
                    if (it != cache.end() &&
                        it->second.fileSize == localSize &&
                        it->second.lastWriteTime == lastWriteTime &&
                        !it->second.hash.empty())
                    {
                        fileHash = it->second.hash;
                        foundInCache = true;
                    }
                }

                if (!foundInCache)
                {
                    // Compute SHA-256 with OpenSSL sequential read
                    fileHash = integrity::createHashFromFile(file.FilePath, file.FileHash);
                    if (!fileHash.empty())
                    {
                        std::lock_guard<std::mutex> lock(cacheMutex);
                        cache[file.FilePath] = CacheEntry{ localSize, lastWriteTime, fileHash };
                        cacheModified.store(true, std::memory_order_relaxed);
                    }
                }

                file.ToUpdate = fileHash.empty() || fileHash != file.FileHash;
                if (file.ToUpdate)
                    updates.fetch_add(1, std::memory_order_relaxed);
            }

            const size_t current = count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress_)
                progress_(current, files.size(), file);
        });

    if (cacheModified.load(std::memory_order_relaxed))
    {
        SaveHashCache(cache);
    }

    return updates.load(std::memory_order_relaxed);
}

