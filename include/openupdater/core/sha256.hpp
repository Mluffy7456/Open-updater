#pragma once

#include <filesystem>
#include <string>

namespace openupdater {

class Sha256 {
public:
    [[nodiscard]] static std::string hash_file(
        const std::filesystem::path& path);
};

[[nodiscard]] bool verify_sha256(
    const std::filesystem::path& path,
    const std::string& expected);

} // namespace openupdater
