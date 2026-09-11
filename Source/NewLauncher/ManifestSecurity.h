#pragma once

#include <string>

namespace manifest_security
{
    std::string Sign(const std::string& payload, const std::string& privateKeyPem) noexcept;
    bool Verify(const std::string& payload,
                const std::string& signatureBase64,
                const std::string& publicKeyPem) noexcept;
}
