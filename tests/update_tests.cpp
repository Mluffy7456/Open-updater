#include "openupdater/core/updater.hpp"

#include <cassert>
#include <filesystem>

int main() {
    using namespace openupdater;

    const auto current = Version("1.2.0");
    const auto available = Version("1.3.0");

    assert(current.valid());
    assert(available.valid());
    assert(available > current);

    const auto check = UpdateCheck{
        current,
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
    assert(std::filesystem::path{}.empty());

    return 0;
}
