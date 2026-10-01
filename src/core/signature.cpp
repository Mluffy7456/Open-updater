#include "openupdater/core/signature.hpp"

#include "openupdater/core/sha256.hpp"

#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <stdexcept>
#include <string>

namespace openupdater {

namespace {

std::string normalize_hex(const std::string& value, std::size_t expected) {
    if (value.size() != expected * 2)
        throw std::invalid_argument("Hex value has an invalid length.");

    std::string result = value;
    for (char& character : result) {
        if (character >= 'A' && character <= 'F')
            character = static_cast<char>(character - 'A' + 'a');

        const bool digit = character >= '0' && character <= '9';
        const bool hex = character >= 'a' && character <= 'f';
        if (!digit && !hex)
            throw std::invalid_argument("Hex value contains an invalid character.");
    }
    return result;
}

std::vector<unsigned char> decode_hex(const std::string& value) {
    std::vector<unsigned char> result;
    result.reserve(value.size() / 2);

    for (std::size_t i = 0; i < value.size(); i += 2) {
        const auto digit = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9')
                return static_cast<unsigned char>(c - '0');
            return static_cast<unsigned char>(c - 'a' + 10);
        };

        result.push_back(
            static_cast<unsigned char>(
                (digit(value[i]) << 4u) | digit(value[i + 1])));
    }

    return result;
}

} // namespace

bool SignatureVerifier::verify_ed25519_sha256(
    const std::filesystem::path& path,
    const std::string& signature_hex,
    const std::string& public_key_hex) {

    const auto signature =
        decode_hex(normalize_hex(signature_hex, 64));
    const auto public_key =
        decode_hex(normalize_hex(public_key_hex, 32));

    const auto digest_hex = Sha256::hash_file(path);
    const auto digest = decode_hex(normalize_hex(digest_hex, 32));

    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519,
        nullptr,
        public_key.data(),
        public_key.size());

    if (!key)
        throw std::runtime_error("Failed to create Ed25519 public key.");

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) {
        EVP_PKEY_free(key);
        throw std::runtime_error("Failed to create signature context.");
    }

    const bool initialized =
        EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key) == 1;

    bool valid = false;
    if (initialized) {
        valid =
            EVP_DigestVerify(
                context,
                signature.data(),
                signature.size(),
                digest.data(),
                digest.size()) == 1;
    }

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);

    return valid;
}

} // namespace openupdater
