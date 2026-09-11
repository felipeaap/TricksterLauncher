#include <cassert>
#include <iostream>
#include <string>
#include "../Source/NewLauncher/Config.h"
#include "../Source/NewLauncher/ManifestManager.h"
#include "../Source/NewLauncher/ManifestSecurity.h"
#include "json.hpp"

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

    // 6. Test ManifestManager strict fail-closed integration
    config::ManifestPublicKeyPem = publicKeyPem;
    config::ManifestRequireSignature = true;

    // A: Signed manifest matches key -> should load successfully
    nlohmann::json validDoc;
    validDoc["version"] = 200;
    validDoc["files"] = nlohmann::json::array({
        { {"FileID", 1}, {"FilePath", "data.pak"}, {"FileHash", "abc"}, {"FileSize", 500LL}, {"ToUpdate", false} }
    });
    std::string validCanonical = validDoc.dump();
    std::string validSig = manifest_security::Sign(validCanonical, privateKeyPem);
    validDoc["signature"] = { {"value", validSig} };

    ManifestManager mgrValid([&](const std::string& path) -> std::string {
        if (path == "/manifest.json")
            return validDoc.dump();
        return {};
    });

    ManifestManager::FileList loadedFiles;
    int localVer = 1;
    int resVer = mgrValid.Load(loadedFiles, false, localVer);
    assert(resVer == 200);
    assert(loadedFiles.size() == 1);
    assert(loadedFiles[0].FilePath == "data.pak");

    // B: Forged/Tampered manifest -> MUST fail-closed when ManifestRequireSignature is true
    nlohmann::json forgedDoc = validDoc;
    forgedDoc["version"] = 999; // tampered version without resigning
    ManifestManager mgrForged([&](const std::string& path) -> std::string {
        if (path == "/manifest.json")
            return forgedDoc.dump();
        return {};
    });

    loadedFiles.clear();
    localVer = 1;
    resVer = mgrForged.Load(loadedFiles, false, localVer);
    assert(loadedFiles.empty()); // failed-closed!

    // C: Missing signature -> MUST fail-closed when ManifestRequireSignature is true
    nlohmann::json unsignedDoc = validDoc;
    unsignedDoc.erase("signature");
    ManifestManager mgrUnsigned([&](const std::string& path) -> std::string {
        if (path == "/manifest.json")
            return unsignedDoc.dump();
        return {};
    });

    loadedFiles.clear();
    localVer = 1;
    resVer = mgrUnsigned.Load(loadedFiles, false, localVer);
    assert(loadedFiles.empty()); // failed-closed!

    // Reset config
    config::ManifestRequireSignature = false;

    std::cout << "[PASS] ManifestSecurity (RSA-PSS) tests passed!" << std::endl;
}
