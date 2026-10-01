#include "openupdater/core/manifest.hpp"
#include "openupdater/core/updater.hpp"
#include "openupdater/core/version.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    using namespace openupdater;

    assert(compare_versions("1.2.0", "1.1.9") > 0);
    assert(compare_versions("1.2", "1.2.0") == 0);
    assert(compare_versions("v2.0.0", "1.99.99") > 0);

    const auto temp =
        std::filesystem::temp_directory_path() / "openupdater_test";
    std::filesystem::remove_all(temp);
    std::filesystem::create_directories(temp);

    const auto manifest_path = temp / "manifest.ff";
    std::ofstream(manifest_path)
        << "application=DemoApp\n"
        << "version=1.4.0\n"
        << "package=DemoApp-1.4.0.zip\n";

    const auto manifest = load_manifest(manifest_path);
    assert(manifest.application == "DemoApp");
    assert(manifest.version == Version("1.4"));
    assert(manifest.package == "DemoApp-1.4.0.zip");

    const auto update = Updater::check(Version("1.3.0"), manifest);
    assert(update.state == UpdateState::UpdateAvailable);

    const auto no_update = Updater::check(Version("1.4.0"), manifest);
    assert(no_update.state == UpdateState::UpToDate);

    const auto package = temp / "package.zip";
    std::ofstream(package) << "test package";

    const auto destination = temp / "installed";
    Updater::install(
        package,
        destination,
        "e6a39ba1c067edf949f7e2906b801d919d1f36f4fd52987d149303e5f11a8607");

    assert(std::filesystem::exists(destination / "package.zip"));

    std::filesystem::remove_all(temp);
    return 0;
}
