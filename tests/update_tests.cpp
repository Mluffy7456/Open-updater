#include "openupdater/core/api.hpp"
#include "openupdater/core/error.hpp"
#include "openupdater/core/platform.hpp"
#include "openupdater/core/updater.hpp"

#include <cassert>

int main() {
    using namespace openupdater;

    static_assert(API_VERSION_MAJOR == 1);
    static_assert(API_VERSION_MINOR == 3);
    static_assert(API_VERSION_PATCH == 0);

    const UpdateRequest request{
        Version("1.2.0"),
        "owner/repository",
        "DemoApp-1.3.0.zip",
        "./updates",
        "",
        {}
    };

    assert(request.current.valid());
    assert(request.options.backup_existing);
    assert(request.options.automatic_rollback);
    assert(request.options.verify_download);
    const UpdateDiscoveryRequest discovery_request{
        "owner/repository",
        {
            {"Core", Version("1.0.0")},
            {"GUI", Version("2.3.0")}
        },
        Platform::Windows,
        Architecture::X64,
        {}
    };

    assert(discovery_request.targets.size() == 2);
    assert(discovery_request.targets[0].component == "Core");
    assert(discovery_request.targets[1].current == Version("2.3.0"));

    const std::vector<AvailableUpdate> discovered{
        {
            "Core",
            Version("1.0.0"),
            Version("1.4.0"),
            "Core-windows-x64.zip",
            "windows",
            "x64",
            "abc",
            "https://example.invalid/core.zip"
        },
        {
            "GUI",
            Version("2.0.0"),
            Version("2.3.0"),
            "GUI-windows-x64.zip",
            "windows",
            "x64",
            "def",
            "https://example.invalid/gui.zip"
        }
    };

    UpdateSelection selection;
    assert(selection.empty());
    selection.select("Core");
    selection.select("Core");
    assert(selection.selected("Core"));
    assert(selection.components.size() == 1);
    selection.select("GUI");
    assert(selection.components.size() == 2);
    selection.deselect("Core");
    assert(!selection.selected("Core"));
    assert(selection.selected("GUI"));
    selection.select_all(discovered);
    assert(selection.components.size() == 2);
    assert(selection.selected("Core"));
    assert(selection.selected("GUI"));
    selection.clear();
    assert(selection.empty());



    const auto platform = current_platform();
    const auto architecture = current_architecture();
    assert(platform != Platform::Unknown);
    assert(architecture != Architecture::Unknown);
    assert(asset_matches_platform("DemoApp-windows-x64.zip", Platform::Windows, Architecture::X64));
    assert(asset_matches_platform("DemoApp-linux-arm64.tar.gz", Platform::Linux, Architecture::Arm64));
    assert(!asset_matches_platform("DemoApp-linux-x64.zip", Platform::Windows, Architecture::X64));
    assert(!asset_matches_platform("DemoApp-windows-x64.zip", Platform::Windows, Architecture::Arm64));

    const auto available = Version("1.3.0");
    const auto check = UpdateCheck{
        request.current,
        available,
        UpdateState::UpdateAvailable
    };

    assert(check.state == UpdateState::UpdateAvailable);

    const auto up_to_date = UpdateCheck{
        available,
        available,
        UpdateState::UpToDate
    };

    assert(up_to_date.state == UpdateState::UpToDate);

    bool discovery_caught = false;
    try {
        Updater::discover_updates(UpdateDiscoveryRequest{
            "invalid",
            {{"Core", Version("1.0.0")}},
            Platform::Windows,
            Architecture::X64,
            {}
        });
    } catch (const UpdateError& error) {
        discovery_caught = true;
        assert(error.code() == ErrorCode::InvalidArgument);
    }
    assert(discovery_caught);

    discovery_caught = false;
    try {
        Updater::discover_updates(UpdateDiscoveryRequest{
            "owner/repository",
            {{"Core", Version("not-a-version")}},
            Platform::Windows,
            Architecture::X64,
            {}
        });
    } catch (const UpdateError& error) {
        discovery_caught = true;
        assert(error.code() == ErrorCode::InvalidVersion);
    }
    assert(discovery_caught);

    bool caught = false;
    try {
        Updater::update(UpdateRequest{
            Version("not-a-version"),
            "owner/repository",
            "DemoApp.zip",
            "./updates",
            "",
            {}
        });
    } catch (const UpdateError& error) {
        caught = true;
        assert(error.code() == ErrorCode::InvalidVersion);
    }
    assert(caught);

    caught = false;
    try {
        Updater::update(UpdateRequest{
            Version("1.0.0"),
            "invalid",
            "DemoApp.zip",
            "./updates",
            "",
            {}
        });
    } catch (const UpdateError& error) {
        caught = true;
        assert(error.code() == ErrorCode::InvalidArgument);
    }
    assert(caught);

    caught = false;
    try {
        Updater::update(UpdateRequest{
            Version("1.0.0"),
            "owner/repository",
            "DemoApp.zip",
            {},
            "",
            {}
        });
    } catch (const UpdateError& error) {
        caught = true;
        assert(error.code() == ErrorCode::InvalidArgument);
    }
    assert(caught);

    return 0;
}
