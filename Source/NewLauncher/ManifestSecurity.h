#pragma once

#include <string>

namespace manifest_security
{
    bool GenerateKeyPair(std::string& outPrivateKeyPem, std::string& outPublicKeyPem) noexcept;
    std::string Sign(const std::string& payload, const std::string& privateKeyPem) noexcept;
    bool Verify(const std::string& payload,
                const std::string& signatureBase64,
                const std::string& publicKeyPem) noexcept;
}

