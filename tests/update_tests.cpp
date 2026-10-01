#include "openupdater/core/api.hpp"
#include "openupdater/core/error.hpp"
#include "openupdater/core/updater.hpp"

#include <cassert>

int main() {
    using namespace openupdater;

    static_assert(API_VERSION_MAJOR == 1);
    static_assert(API_VERSION_MINOR == 0);
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
