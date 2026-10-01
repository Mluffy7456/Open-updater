#pragma once

#include <filesystem>
#include <string>

namespace openupdater {

class SignatureVerifier {
public:
    // Verifies an Ed25519 signature over the raw SHA-256 digest of the file.
    // signature_hex must contain 64 bytes / 128 hexadecimal characters.
    // public_key_hex must contain 32 bytes / 64 hexadecimal characters.
    [[nodiscard]] static bool verify_ed25519_sha256(
        const std::filesystem::path& path,
        const std::string& signature_hex,
        const std::string& public_key_hex);
};

} // namespace openupdater
