#include "openupdater/core/universal.hpp"

#include "openupdater/providers/winget.hpp"
#include "openupdater/providers/windows_update.hpp"

#include <stdexcept>
#include <utility>

namespace openupdater {

UniversalUpdateManager::UniversalUpdateManager() {
#ifdef _WIN32
    providers_.push_back(std::make_unique<WinGetProvider>());
    providers_.push_back(std::make_unique<WindowsUpdateProvider>());
#endif
}

UpdateScanResult UniversalUpdateManager::check_all() {
    UpdateScanResult result;

    for (const auto& provider : providers_) {
        if (!provider->available())
            continue;

        try {
            auto updates = provider->check_updates();
            result.updates.insert(
                result.updates.end(),
                std::make_move_iterator(updates.begin()),
                std::make_move_iterator(updates.end()));
        } catch (const std::exception& error) {
            result.provider_errors.push_back(
                provider->name() + ": " + error.what());
        }
    }

    return result;
}

void UniversalUpdateManager::install(const DiscoveredUpdate& update) {
    for (const auto& provider : providers_) {
        if (provider->name() == update.provider) {
            provider->install(update);
            return;
        }
    }

    throw std::runtime_error(
        "No update provider is registered for: " + update.provider);
}

const char* update_category_name(UpdateCategory category) noexcept {
    switch (category) {
    case UpdateCategory::Application: return "Applications";
    case UpdateCategory::Programming: return "Programming";
    case UpdateCategory::Driver: return "Drivers";
    case UpdateCategory::Runtime: return "Runtimes";
    case UpdateCategory::System: return "System";
    default: return "Other";
    }
}

const char* update_source_name(UpdateSource source) noexcept {
    switch (source) {
    case UpdateSource::WinGet: return "WinGet";
    case UpdateSource::WindowsUpdate: return "Windows Update";
    case UpdateSource::GitHub: return "GitHub";
    case UpdateSource::PackageManager: return "Package manager";
    default: return "Other";
    }
}

} // namespace openupdater
