#pragma once

#include <memory>
#include <string>
#include <vector>

namespace openupdater {

enum class UpdateCategory {
    Application,
    Programming,
    Driver,
    Runtime,
    System,
    Other
};

enum class UpdateSource {
    WinGet,
    WindowsUpdate,
    GitHub,
    PackageManager,
    Other
};

struct DiscoveredUpdate {
    std::string id;
    std::string name;
    std::string current_version;
    std::string available_version;
    UpdateCategory category = UpdateCategory::Other;
    UpdateSource source = UpdateSource::Other;
    std::string provider;
    std::string publisher;
    std::string platform;
    std::string architecture;
    bool requires_admin = false;
    bool requires_restart = false;
};

class IUpdateProvider {
public:
    virtual ~IUpdateProvider() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool available() const = 0;
    [[nodiscard]] virtual std::vector<DiscoveredUpdate> check_updates() = 0;
    virtual void install(const DiscoveredUpdate& update) = 0;
};

struct UpdateScanResult {
    std::vector<DiscoveredUpdate> updates;
    std::vector<std::string> provider_errors;
};

class UniversalUpdateManager {
public:
    UniversalUpdateManager();

    [[nodiscard]] UpdateScanResult check_all();
    void install(const DiscoveredUpdate& update);

private:
    std::vector<std::unique_ptr<IUpdateProvider>> providers_;
};

[[nodiscard]] const char* update_category_name(UpdateCategory category) noexcept;
[[nodiscard]] const char* update_source_name(UpdateSource source) noexcept;

} // namespace openupdater
