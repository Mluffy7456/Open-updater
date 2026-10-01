#include "openupdater/core/github.hpp"
#include "openupdater/core/manifest.hpp"
#include "openupdater/core/backup.hpp"
#include "openupdater/core/updater.hpp"
#include "openupdater/core/version.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage() {
    std::cout
        << "OpenUpdater 1.1.0\n"
        << "Usage:\n"
        << "  openupdater version <version>\n"
        << "  openupdater check <current-version> <manifest>\n"
        << "  openupdater install <package> <destination> [sha256]\n"
        << "  openupdater rollback <backup> <destination>\n"
        << "  openupdater verify <package> <sha256>\n"
        << "  openupdater download <url> <destination>\n"
        << "  openupdater github <owner/repository> <asset> <destination> [sha256]\n"
        << "  openupdater update [--silent] <current-version> <owner/repository> <asset> <destination> [sha256]\n";
}

}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            print_usage();
            return 1;
        }

        const std::string command = argv[1];

        if (command == "version") {
            if (argc != 3) {
                print_usage();
                return 1;
            }

            const openupdater::Version version(argv[2]);
            if (!version.valid()) {
                std::cerr << "Invalid version.\n";
                return 2;
            }

            std::cout << version.str() << '\n';
            return 0;
        }

        if (command == "check") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            const openupdater::Version current(argv[2]);
            const auto manifest = openupdater::load_manifest(argv[3]);
            const auto result = openupdater::Updater::check(current, manifest);

            std::cout << "Current:   " << result.current.str() << '\n';
            std::cout << "Available: " << result.available.str() << '\n';

            if (result.state == openupdater::UpdateState::UpdateAvailable) {
                std::cout << "Update available.\n";
                return 10;
            }

            std::cout << "Already up to date.\n";
            return 0;
        }

        if (command == "github") {
            if (argc != 5 && argc != 6) {
                print_usage();
                return 1;
            }

            const std::string expected_sha256 = argc == 6 ? argv[5] : "";
            const auto release = openupdater::GitHubReleasesProvider::download_latest(
                argv[2], argv[3], argv[4], expected_sha256);

            std::cout << "Release: " << release.tag << '\n';
            std::cout << "Asset:   " << release.asset << '\n';
            if (!release.sha256.empty())
                std::cout << "SHA-256: " << release.sha256 << '\n';
            std::cout << "GitHub asset downloaded successfully.\n";
            return 0;
        }

        if (command == "download") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            openupdater::Downloader::download(argv[2], argv[3]);
            std::cout << "Download completed successfully.\n";
            return 0;
        }

        if (command == "install") {
            if (argc != 4 && argc != 5) {
                print_usage();
                return 1;
            }

            const std::string expected_sha256 = argc == 5 ? argv[4] : "";
            const auto backup = openupdater::Updater::install(
                argv[2], argv[3], expected_sha256);

            std::cout << "Package installed successfully.\n";
            if (!backup.empty())
                std::cout << "Backup: " << backup.string() << '\n';
            return 0;
        }

        if (command == "rollback") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            openupdater::BackupManager::rollback(argv[2], argv[3]);
            std::cout << "Rollback completed successfully.\n";
            return 0;
        }

        if (command == "verify") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            const auto actual = openupdater::Sha256::hash_file(argv[2]);
            std::cout << "SHA-256: " << actual << '\n';

            if (!openupdater::verify_sha256(argv[2], argv[3])) {
                std::cerr << "SHA-256 verification failed.\n";
                return 3;
            }

            std::cout << "SHA-256 verification passed.\n";
            return 0;
        }

        if (command == "update") {
            bool silent = false;
            int first_argument = 2;

            if (first_argument < argc &&
                std::string(argv[first_argument]) == "--silent") {
                silent = true;
                ++first_argument;
            }

            const int remaining = argc - first_argument;
            if (remaining != 4 && remaining != 5) {
                print_usage();
                return 1;
            }

            const std::string expected_sha256 =
                remaining == 5 ? argv[first_argument + 4] : "";

            const openupdater::Version current(argv[first_argument]);
            openupdater::UpdateRequest request{
                current,
                argv[first_argument + 1],
                argv[first_argument + 2],
                argv[first_argument + 3],
                expected_sha256,
                {}
            };

            const auto result = openupdater::Updater::update(request);

            if (result.state == openupdater::UpdateState::UpToDate) {
                if (!silent)
                    std::cout << "Already up to date: "
                              << result.available.str() << '\n';
                return 10;
            }

            if (!silent) {
                std::cout << "Updated to " << result.available.str() << ".\n";
                if (!result.backup.empty())
                    std::cout << "Backup: " << result.backup.string() << '\n';
            }

            return 0;
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
