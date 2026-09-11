#include "Integrity.h"

#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>

namespace
{
std::string DigestFile(const std::string& filePath, const EVP_MD* algorithm) noexcept
{
    if (!algorithm)
        return {};

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return {};

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
        return {};

    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digestLength = 0;
    std::array<char, 1024 * 1024> buffer{};
    bool success = EVP_DigestInit_ex(context, algorithm, nullptr) == 1;

    while (success && file.good())
    {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read = file.gcount();
        if (read > 0)
            success = EVP_DigestUpdate(context, buffer.data(), static_cast<size_t>(read)) == 1;
    }

    if (success)
        success = EVP_DigestFinal_ex(context, digest, &digestLength) == 1;

    EVP_MD_CTX_free(context);
    if (!success)
        return {};

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLength; ++i)
        result << std::setw(2) << static_cast<unsigned int>(digest[i]);

    return result.str();
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
