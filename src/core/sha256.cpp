#include "openupdater/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace openupdater {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

constexpr std::array<std::uint32_t, 8> kInitialState{
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

constexpr std::uint32_t rotate_right(std::uint32_t value, unsigned count) {
    return (value >> count) | (value << (32u - count));
}

constexpr std::uint32_t choose(
    std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

constexpr std::uint32_t majority(
    std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr std::uint32_t big_sigma0(std::uint32_t x) {
    return rotate_right(x, 2) ^ rotate_right(x, 13) ^ rotate_right(x, 22);
}

constexpr std::uint32_t big_sigma1(std::uint32_t x) {
    return rotate_right(x, 6) ^ rotate_right(x, 11) ^ rotate_right(x, 25);
}

constexpr std::uint32_t small_sigma0(std::uint32_t x) {
    return rotate_right(x, 7) ^ rotate_right(x, 18) ^ (x >> 3);
}

constexpr std::uint32_t small_sigma1(std::uint32_t x) {
    return rotate_right(x, 17) ^ rotate_right(x, 19) ^ (x >> 10);
}

void transform(
    std::array<std::uint32_t, 8>& state,
    const std::array<std::uint8_t, 64>& block) {

    std::array<std::uint32_t, 64> words{};

    for (std::size_t i = 0; i < 16; ++i) {
        const auto offset = i * 4;
        words[i] =
            (static_cast<std::uint32_t>(block[offset]) << 24u) |
            (static_cast<std::uint32_t>(block[offset + 1]) << 16u) |
            (static_cast<std::uint32_t>(block[offset + 2]) << 8u) |
            static_cast<std::uint32_t>(block[offset + 3]);
    }

    for (std::size_t i = 16; i < words.size(); ++i) {
        words[i] =
            small_sigma1(words[i - 2]) + words[i - 7] +
            small_sigma0(words[i - 15]) + words[i - 16];
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    auto f = state[5];
    auto g = state[6];
    auto h = state[7];

    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto temp1 =
            h + big_sigma1(e) + choose(e, f, g) +
            kRoundConstants[i] + words[i];
        const auto temp2 = big_sigma0(a) + majority(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

std::string state_to_hex(const std::array<std::uint32_t, 8>& state) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');

    for (const auto word : state)
        output << std::setw(8) << word;

    return output.str();
}

std::string normalize_expected(std::string value) {
    if (value.size() != 64)
        throw std::invalid_argument(
            "SHA-256 digest must contain exactly 64 hexadecimal characters.");

    for (char& character : value) {
        if (character >= 'A' && character <= 'F')
            character = static_cast<char>(character - 'A' + 'a');

        const bool is_digit = character >= '0' && character <= '9';
        const bool is_lower_hex =
            character >= 'a' && character <= 'f';

        if (!is_digit && !is_lower_hex)
            throw std::invalid_argument(
                "SHA-256 digest contains a non-hexadecimal character.");
    }

    return value;
}

} // namespace

std::string Sha256::hash_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open file for SHA-256: " + path.string());

    std::array<std::uint32_t, 8> state = kInitialState;
    std::array<std::uint8_t, 64> block{};
    std::uint64_t total_size = 0;

    while (true) {
        input.read(
            reinterpret_cast<char*>(block.data()),
            static_cast<std::streamsize>(block.size()));

        const auto count = input.gcount();
        if (count > 0) {
            total_size += static_cast<std::uint64_t>(count);

            if (count == static_cast<std::streamsize>(block.size())) {
                transform(state, block);
                continue;
            }

            for (std::streamsize i = count; i < static_cast<std::streamsize>(block.size()); ++i)
                block[static_cast<std::size_t>(i)] = 0;

            block[static_cast<std::size_t>(count)] = 0x80u;

            if (count >= 56) {
                transform(state, block);
                block.fill(0);
            }

            const auto bit_length = total_size * 8u;
            for (unsigned i = 0; i < 8; ++i) {
                block[63u - i] =
                    static_cast<std::uint8_t>(bit_length >> (i * 8u));
            }

            transform(state, block);
            return state_to_hex(state);
        }

        if (input.eof()) {
            block.fill(0);
            block[0] = 0x80u;

            if (56 <= 0) {
                // Unreachable; kept out of the normal padding path.
            }

            const auto bit_length = total_size * 8u;
            for (unsigned i = 0; i < 8; ++i) {
                block[63u - i] =
                    static_cast<std::uint8_t>(bit_length >> (i * 8u));
            }

            transform(state, block);
            return state_to_hex(state);
        }

        if (input.fail())
            throw std::runtime_error("Failed to read file for SHA-256: " + path.string());
    }
}

bool verify_sha256(
    const std::filesystem::path& path,
    const std::string& expected) {

    const auto normalized = normalize_expected(expected);
    return Sha256::hash_file(path) == normalized;
}

} // namespace openupdater
