#pragma once

#include <filesystem>

namespace openupdater {

struct BackupInfo {
    std::filesystem::path original;
    std::filesystem::path backup;
};

class BackupManager {
public:
    [[nodiscard]] static BackupInfo create(
        const std::filesystem::path& source,
        const std::filesystem::path& backup_directory);

    static void rollback(
        const std::filesystem::path& backup,
        const std::filesystem::path& destination);
};

} // namespace openupdater
