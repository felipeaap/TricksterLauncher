# Manifest Signing

The launcher supports RSA digital signatures on the manifest to prevent tampering during CDN distribution.

## Overview

When `ManifestRequireSignature = true` in `Config.cpp`, the launcher will:
1. Download the manifest from the CDN
2. Verify the RSA-PSS signature against the embedded public key
3. Refuse to process the manifest if the signature is invalid

## Key Generation

Generate an RSA-2048 key pair using OpenSSL:

```bash
# Generate private key (KEEP SECRET — never commit to repo)
openssl genpkey -algorithm RSA -out manifest_private.pem -pkeyopt rsa_keygen_bits:2048

# Extract public key
openssl rsa -in manifest_private.pem -pubout -out manifest_public.pem
```

## Signing the Manifest

After generating `manifest.json` with FileListGen, sign it:

```bash
# Create signature file (SHA-256 + PSS padding)
openssl dgst -sha256 -sigopt rsa_padding_mode:pss -sign manifest_private.pem \
  -out manifest.sig manifest.json

# Base64 encode the signature for embedding
openssl base64 -in manifest.sig -out manifest.sig.b64
```

## Configuring the Launcher

In [`Config.cpp`](../Source/NewLauncher/Config.cpp), set:

```cpp
bool ManifestRequireSignature = true;
std::string ManifestPublicKeyPem = R"(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...
-----END PUBLIC KEY-----
)";
```

## Verification Flow

```
CDN                          Launcher
 |                              |
 |  GET /manifest.json          |
 |<-----------------------------|
 |  GET /manifest.sig           |
 |<-----------------------------|
 |                              |
 |  manifest.json + sig         |
 |----------------------------->|
 |                              |
 |              RSA-PSS-SHA256  |
 |              verify(pubkey,  |
 |                sig, hash)    |
 |              ✓ proceed       |
 |              ✗ abort update  |
```

## Example RSA-2048 Key Pair (for development only)

> **⚠️ WARNING**: These keys are for local testing only. **Never use example keys in production.** Generate your own key pair and store the private key securely outside the repository.

### Public Key (embed in Config.cpp for testing)
```
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAu4qkR9s1xwD0YEkO3qwT
2xJFn7N0j6g3E8zR0QxaP5f8K2zC8bP1qR5nM3wY9dX7jL6uH1vA8cE4iO0tS2mN
7kB5xQ3pD9gF1hJ6rL8wU0eY4bI2aT5oK3nV7cM9fZ0xW1yR6dP4sG8jH2lQ5mA3
tE7iU0oP9rK1cS6vL4wX2bN5fY8gJ3hM0dQ7nT6aR4eO9pI1uV2cW5xZ0jK3lA8
sB7mD4fG6hN9qE2tY0iR3oP5wJ1cX7vL4bK6nM8gT0aS9dU2eQ5rW3yH7zF1xI4
OQIDAQAB
-----END PUBLIC KEY-----
```

### Private Key (store securely, NEVER commit)
Generate your own using the commands above.

## Security Checklist

- [ ] Private key is stored in a secure vault (e.g., AWS KMS, HashiCorp Vault)
- [ ] Private key is **never** committed to the repository
- [ ] CI/CD pipeline signs the manifest automatically using a secrets manager
- [ ] Public key is embedded in `Config.cpp` and compiled into the binary
- [ ] `ManifestRequireSignature` is set to `true` in production builds
