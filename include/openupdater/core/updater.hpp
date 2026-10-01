#pragma once

#include "openupdater/core/api.hpp"
#include "openupdater/core/manifest.hpp"
#include "openupdater/core/sha256.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace openupdater {

struct UpdateCheck {
    Version current;
    Version available;
    UpdateState state;
};

struct UpdateResult {
    Version current;
    Version available;
    UpdateState state;
    std::filesystem::path backup;
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

class Downloader {
public:
    static void download(
        const std::string& url,
        const std::filesystem::path& destination,
        const HttpHeaders& headers = {});
};

class Updater {
public:
    [[nodiscard]] static UpdateCheck check(
        const Version& current,
        const Manifest& manifest);

    [[nodiscard]] static std::filesystem::path install(
        const std::filesystem::path& package,
        const std::filesystem::path& destination,
        const std::string& expected_sha256 = {});

    [[nodiscard]] static UpdateResult update_from_github(
        const Version& current,
        const std::string& repository,
        const std::string& asset_name,
        const std::filesystem::path& destination,
        const std::string& expected_sha256 = {});

    [[nodiscard]] static UpdateReport update(
        const UpdateRequest& request);
};

} // namespace openupdater
