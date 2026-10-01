#pragma once

#include "openupdater/core/error.hpp"
#include "openupdater/core/version.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace openupdater {

inline constexpr int API_VERSION_MAJOR = 1;
inline constexpr int API_VERSION_MINOR = 1;
inline constexpr int API_VERSION_PATCH = 0;
inline constexpr const char* API_VERSION = "1.1.0";

enum class UpdateState {
    UpToDate,
    UpdateAvailable
};

using ApiHttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct UpdateOptions {
    bool backup_existing = true;
    bool automatic_rollback = true;
    bool verify_download = true;
    ApiHttpHeaders headers{};
};

struct UpdateRequest {
    Version current;
    std::string repository;
    std::string asset_name;
    std::filesystem::path destination;
    std::string expected_sha256;
    UpdateOptions options{};
};

struct UpdateReport {
    Version current;
    Version available;
    UpdateState state;
    std::filesystem::path package;
    std::filesystem::path backup;
};

} // namespace openupdater
