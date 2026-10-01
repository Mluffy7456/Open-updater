#pragma once

#include "openupdater/core/error.hpp"
#include "openupdater/core/platform.hpp"
#include "openupdater/core/signature.hpp"
#include "openupdater/core/version.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace openupdater {

inline constexpr int API_VERSION_MAJOR = 1;
inline constexpr int API_VERSION_MINOR = 3;
inline constexpr int API_VERSION_PATCH = 0;
inline constexpr const char* API_VERSION = "1.3.0";

enum class UpdateState {
    UpToDate,
    UpdateAvailable
};

using ApiHttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct UpdateOptions {
    bool backup_existing = true;
    bool automatic_rollback = true;
    bool verify_download = true;
    bool verify_signature = false;
    std::string trusted_public_key{};
    ApiHttpHeaders headers{};
};


struct UpdateTarget {
    std::string component;
    Version current;
};

struct AvailableUpdate {
    std::string component;
    Version current;
    Version available;
    std::string asset;
    std::string platform;
    std::string architecture;
    std::string sha256;
    std::string signature_url;
    std::string download_url;
};

struct UpdateSelection {
    std::vector<std::string> components;

    void select(const std::string& component);
    void deselect(const std::string& component);
    void clear();
    void select_all(const std::vector<AvailableUpdate>& updates);

    [[nodiscard]] bool selected(const std::string& component) const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

struct SelectedUpdateRequest {
    std::vector<AvailableUpdate> updates;
    UpdateSelection selection;
    std::filesystem::path destination;
    UpdateOptions options{};
};

struct UpdateDiscoveryRequest {
    std::string repository;
    std::vector<UpdateTarget> targets;
    Platform platform = current_platform();
    Architecture architecture = current_architecture();
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
