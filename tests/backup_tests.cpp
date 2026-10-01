#include "openupdater/core/backup.hpp"
#include "openupdater/core/updater.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    using namespace openupdater;

    const auto temp =
        std::filesystem::temp_directory_path() / "openupdater_backup_test";
    std::filesystem::remove_all(temp);
    std::filesystem::create_directories(temp);

    const auto package = temp / "DemoApp.zip";
    const auto destination = temp / "installed";
    std::filesystem::create_directories(destination);

    const auto target = destination / package.filename();
    std::ofstream(target) << "old package";
    std::ofstream(package) << "new package";

    const auto backup_directory =
        destination / ".openupdater" / "backups";

    const auto backup = BackupManager::create(target, backup_directory);
    assert(backup.original == std::filesystem::absolute(target));
    assert(std::filesystem::exists(backup.backup));

    Updater::install(package, destination);
    {
        std::ifstream input(target);
        std::string content;
        input >> content;
        assert(content == "new");
    }

    BackupManager::rollback(backup.backup, target);
    {
        std::ifstream input(target);
        std::string content;
        input >> content;
        assert(content == "old");
    }

    std::filesystem::remove_all(temp);
    return 0;
}
