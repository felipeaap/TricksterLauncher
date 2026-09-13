#define NOMINMAX
#include "Integrity.h"

#include <windows.h>
#include <array>
#include <cctype>
#include <string>
#include <openssl/evp.h>

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

std::string DigestFile(const std::string& filePath, const EVP_MD* algorithm) noexcept
{
    if (!algorithm)
        return {};

    const std::wstring widePath = Utf8ToWide(filePath);
    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return {};

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
    {
        CloseHandle(hFile);
        return {};
    }

    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digestLength = 0;
    std::array<unsigned char, 131072> buffer{}; // 128KB buffer for optimal I/O throughput
    bool success = EVP_DigestInit_ex(context, algorithm, nullptr) == 1;

    while (success)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
        {
            success = false;
            break;
        }

        if (bytesRead == 0)
            break;

        success = EVP_DigestUpdate(context, buffer.data(), bytesRead) == 1;
    }

    if (success)
        success = EVP_DigestFinal_ex(context, digest, &digestLength) == 1;

    EVP_MD_CTX_free(context);
    CloseHandle(hFile);

    if (!success)
        return {};

    static constexpr char hexChars[] = "0123456789abcdef";
    std::string result;
    result.resize(static_cast<size_t>(digestLength) * 2);
    for (unsigned int i = 0; i < digestLength; ++i)
    {
        result[i * 2] = hexChars[(digest[i] >> 4) & 0x0F];
        result[i * 2 + 1] = hexChars[digest[i] & 0x0F];
    }

    return result;
}

std::string Lower(std::string value)
{
    for (char& character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}
}

namespace integrity
{
std::string createMD5FromFile(const std::string& filePath) noexcept
{
    return DigestFile(filePath, EVP_md5());
}

std::string createSHA256FromFile(const std::string& filePath) noexcept
{
    return DigestFile(filePath, EVP_sha256());
}

std::string createHashFromFile(const std::string& filePath, const std::string& expectedHash) noexcept
{
    const std::string normalized = Lower(expectedHash);
    if (normalized.size() == 64)
        return createSHA256FromFile(filePath);

    return createMD5FromFile(filePath);
}
}

