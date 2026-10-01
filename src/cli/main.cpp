#include "openupdater/core/manifest.hpp"
#include "openupdater/core/updater.hpp"
#include "openupdater/core/version.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage() {
    std::cout
        << "OpenUpdater 0.1.0\n"
        << "Usage:\n"
        << "  openupdater version <version>\n"
        << "  openupdater check <current-version> <manifest>\n"
        << "  openupdater install <package> <destination>\n";
}

} // namespace

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

        if (command == "install") {
            if (argc != 4) {
                print_usage();
                return 1;
            }

            openupdater::Updater::install(argv[2], argv[3]);
            std::cout << "Package installed successfully.\n";
            return 0;
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
