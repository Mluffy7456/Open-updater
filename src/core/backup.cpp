#include "openupdater/core/backup.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace openupdater {

namespace {

std::string backup_suffix() {
    const auto now =
        std::chrono::system_clock::now().time_since_epoch().count();
    return std::to_string(now);
}

} // namespace

BackupInfo BackupManager::create(
    const std::filesystem::path& source,
    const std::filesystem::path& backup_directory) {

    if (!std::filesystem::exists(source))
        throw std::runtime_error(
            "Backup source does not exist: " + source.string());

    if (!std::filesystem::is_regular_file(source))
        throw std::runtime_error(
            "Backup source is not a regular file: " + source.string());

    if (backup_directory.empty())
        throw std::runtime_error("Backup directory cannot be empty.");

    std::filesystem::create_directories(backup_directory);

    const auto original = std::filesystem::absolute(source);
    auto backup = backup_directory /
        (source.filename().string() + "." + backup_suffix() + ".bak");

    unsigned counter = 0;
    while (std::filesystem::exists(backup)) {
        backup = backup_directory /
            (source.filename().string() + "." + backup_suffix() +
             "." + std::to_string(++counter) + ".bak");
    }

    std::error_code error;
    std::filesystem::copy_file(
        source,
        backup,
        std::filesystem::copy_options::none,
        error);

    if (error)
        throw std::runtime_error(
            "Failed to create backup: " + error.message());

    return {original, std::filesystem::absolute(backup)};
}

void BackupManager::rollback(
    const std::filesystem::path& backup,
    const std::filesystem::path& destination) {

    if (!std::filesystem::exists(backup))
        throw std::runtime_error(
            "Backup does not exist: " + backup.string());

    if (!std::filesystem::is_regular_file(backup))
        throw std::runtime_error(
            "Backup is not a regular file: " + backup.string());

    if (destination.empty())
        throw std::runtime_error("Rollback destination cannot be empty.");

    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path());

    std::error_code error;
    std::filesystem::copy_file(
        backup,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        error);

    if (error)
        throw std::runtime_error(
            "Failed to restore backup: " + error.message());
}

} // namespace openupdater
