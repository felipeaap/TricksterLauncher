#include "ManifestSecurity.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <vector>

namespace
{
std::string Base64Encode(const unsigned char* data, size_t size) noexcept
{
    if (!data || size == 0)
        return {};

    const int length = 4 * static_cast<int>((size + 2) / 3);
    std::string encoded(static_cast<size_t>(length), '\0');
    const int written = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()), data, static_cast<int>(size));
    if (written <= 0)
        return {};

    encoded.resize(static_cast<size_t>(written));
    return encoded;
}

std::vector<unsigned char> Base64Decode(const std::string& value) noexcept
{
    if (value.empty())
        return {};

    std::vector<unsigned char> decoded((value.size() * 3) / 4 + 3);
    const int written = EVP_DecodeBlock(
        decoded.data(),
        reinterpret_cast<const unsigned char*>(value.data()),
        static_cast<int>(value.size()));
    if (written < 0)
        return {};

    size_t padding = 0;
    if (!value.empty() && value.back() == '=') ++padding;
    if (value.size() > 1 && value[value.size() - 2] == '=') ++padding;

    decoded.resize(static_cast<size_t>(written) - padding);
    return decoded;
}
}

namespace manifest_security
{
std::string Sign(const std::string& payload, const std::string& privateKeyPem) noexcept
{
    if (payload.empty() || privateKeyPem.empty())
        return {};

    BIO* bio = BIO_new_mem_buf(privateKeyPem.data(), static_cast<int>(privateKeyPem.size()));
    if (!bio)
        return {};

    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!key)
        return {};

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
    {
        EVP_PKEY_free(key);
        return {};
    }

    bool success = EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, key) == 1;
    if (success)
        success = EVP_DigestSignUpdate(context, payload.data(), payload.size()) == 1;

    size_t signatureSize = 0;
    if (success)
        success = EVP_DigestSignFinal(context, nullptr, &signatureSize) == 1;

    std::vector<unsigned char> signature(signatureSize);
    if (success)
        success = EVP_DigestSignFinal(context, signature.data(), &signatureSize) == 1;

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);

    return success ? Base64Encode(signature.data(), signatureSize) : std::string{};
}

bool Verify(const std::string& payload,
            const std::string& signatureBase64,
            const std::string& publicKeyPem) noexcept
{
    if (payload.empty() || signatureBase64.empty() || publicKeyPem.empty())
        return false;

    const auto signature = Base64Decode(signatureBase64);
    if (signature.empty())
        return false;

    BIO* bio = BIO_new_mem_buf(publicKeyPem.data(), static_cast<int>(publicKeyPem.size()));
    if (!bio)
        return false;

    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!key)
        return false;

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
    {
        EVP_PKEY_free(key);
        return false;
    }

    bool success = EVP_DigestVerifyInit(context, nullptr, EVP_sha256(), nullptr, key) == 1;
    if (success)
        success = EVP_DigestVerifyUpdate(context, payload.data(), payload.size()) == 1;
    if (success)
    {
        success = EVP_DigestVerifyFinal(
            context, signature.data(), signature.size()) == 1;
    }

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return success;
}
}
