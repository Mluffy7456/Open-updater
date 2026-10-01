#pragma once

#include "openupdater/core/manifest.hpp"
#include "openupdater/core/sha256.hpp"

#include <filesystem>
#include <string>

namespace openupdater {

enum class UpdateState {
    UpToDate,
    UpdateAvailable
};

struct UpdateCheck {
    Version current;
    Version available;
    UpdateState state;
};

class Downloader {
public:
    static void download(
        const std::string& url,
        const std::filesystem::path& destination);
};

class Updater {
public:
    [[nodiscard]] static UpdateCheck check(
        const Version& current,
        const Manifest& manifest);

    static void install(
        const std::filesystem::path& package,
        const std::filesystem::path& destination,
        const std::string& expected_sha256 = {});
};

} // namespace openupdater
