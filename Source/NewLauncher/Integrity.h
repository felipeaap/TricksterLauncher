#pragma once

#include <string>

namespace integrity
{
    std::string createMD5FromFile(const std::string& filePath) noexcept;
    std::string createSHA256FromFile(const std::string& filePath) noexcept;
    std::string createHashFromFile(const std::string& filePath, const std::string& expectedHash = {}) noexcept;
}
