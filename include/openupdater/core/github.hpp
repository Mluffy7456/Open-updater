#pragma once

#include "openupdater/core/api.hpp"
#include "openupdater/core/platform.hpp"
#include "openupdater/core/version.hpp"

#include <filesystem>
#include <string>

namespace openupdater {

struct GitHubRelease {
    Version version;
    std::string tag;
    std::string asset;
    std::string download_url;
    std::string sha256;
};

class GitHubReleasesProvider {
public:
    [[nodiscard]] static GitHubRelease latest(
        const std::string& repository,
        const std::string& asset_name,
        const ApiHttpHeaders& headers = {});

    // Select an asset using OpenUpdater's platform/architecture naming
    // convention: <component>-<platform>-<architecture>.<extension>.
    [[nodiscard]] static GitHubRelease latest_compatible(
        const std::string& repository,
        const std::string& component,
        Platform platform = current_platform(),
        Architecture architecture = current_architecture(),
        const ApiHttpHeaders& headers = {});

    static GitHubRelease download_latest(
        const std::string& repository,
        const std::string& asset_name,
        const std::filesystem::path& destination,
        const std::string& expected_sha256 = {},
        const ApiHttpHeaders& headers = {});
};

} // namespace openupdater
