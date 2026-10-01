#include "openupdater/core/updater.hpp"

#include <filesystem>
#include <stdexcept>

namespace openupdater {

UpdateCheck Updater::check(const Version& current, const Manifest& manifest) {
    if (!current.valid())
        throw std::runtime_error("Current version is invalid.");

    if (manifest.version > current)
        return {current, manifest.version, UpdateState::UpdateAvailable};

    return {current, manifest.version, UpdateState::UpToDate};
}

void Updater::install(
    const std::filesystem::path& package,
    const std::filesystem::path& destination) {

    if (!std::filesystem::exists(package))
        throw std::runtime_error("Package does not exist: " + package.string());

    std::filesystem::create_directories(destination);

    const auto target = destination / package.filename();
    std::error_code ec;
    std::filesystem::copy_file(
        package,
        target,
        std::filesystem::copy_options::overwrite_existing,
        ec);

    if (ec)
        throw std::runtime_error("Failed to install package: " + ec.message());
}

} // namespace openupdater
