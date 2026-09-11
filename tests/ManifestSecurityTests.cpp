#include <cassert>
#include <iostream>
#include <string>
#include "../Source/NewLauncher/ManifestSecurity.h"

void RunManifestSecurityTests()
{
    std::cout << "[TEST] Running ManifestSecurity (RSA-PSS) tests..." << std::endl;

    std::string privateKeyPem;
    std::string publicKeyPem;

    // 1. Generate key pair
    bool generated = manifest_security::GenerateKeyPair(privateKeyPem, publicKeyPem);
    assert(generated);
    assert(!privateKeyPem.empty());
    assert(!publicKeyPem.empty());
    assert(privateKeyPem.find("BEGIN PRIVATE KEY") != std::string::npos ||
           privateKeyPem.find("BEGIN RSA PRIVATE KEY") != std::string::npos);
    assert(publicKeyPem.find("BEGIN PUBLIC KEY") != std::string::npos);

    // 2. Sign and verify valid payload
    const std::string manifestJson = R"({"version": 100, "files": [{"path": "game.exe", "size": 1024}]})";
    std::string signature = manifest_security::Sign(manifestJson, privateKeyPem);
    assert(!signature.empty());

    bool verified = manifest_security::Verify(manifestJson, signature, publicKeyPem);
    assert(verified);

    // 3. Reject tampered payload
    const std::string tamperedJson = R"({"version": 101, "files": [{"path": "game.exe", "size": 1024}]})";
    bool tamperedVerified = manifest_security::Verify(tamperedJson, signature, publicKeyPem);
    assert(!tamperedVerified);

    // 4. Reject tampered signature
    std::string badSignature = signature;
    if (!badSignature.empty())
    {
        badSignature[0] = (badSignature[0] == 'A') ? 'B' : 'A';
        bool badSigVerified = manifest_security::Verify(manifestJson, badSignature, publicKeyPem);
        assert(!badSigVerified);
    }

    // 5. Reject empty inputs gracefully
    assert(manifest_security::Sign("", privateKeyPem).empty());
    assert(manifest_security::Sign(manifestJson, "").empty());
    assert(!manifest_security::Verify("", signature, publicKeyPem));
    assert(!manifest_security::Verify(manifestJson, "", publicKeyPem));
    assert(!manifest_security::Verify(manifestJson, signature, ""));

    std::cout << "[PASS] ManifestSecurity (RSA-PSS) tests passed!" << std::endl;
}
