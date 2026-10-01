#include "openupdater/core/signature.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    using namespace openupdater;

    const auto path =
        std::filesystem::temp_directory_path() /
        "openupdater_signature_test.bin";

    {
        std::ofstream output(path, std::ios::binary);
    }

    // RFC 8032 Ed25519 public key, signing the SHA-256 digest of an empty file.
    const std::string public_key =
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";

    const std::string signature =
        "48a96e8f6ca118b391bcec11dea165d4ecbcbb81f699bef153edee8a63e40468"
        "b688730c1ba7467bfb114b2c0a5a87b5f07b14597a2535d3f72c07b8ab1c3c07";

    assert(SignatureVerifier::verify_ed25519_sha256(
        path, signature, public_key));

    auto invalid_signature = signature;
    invalid_signature[0] = invalid_signature[0] == '0' ? '1' : '0';

    assert(!SignatureVerifier::verify_ed25519_sha256(
        path, invalid_signature, public_key));

    std::filesystem::remove(path);
    return 0;
}
