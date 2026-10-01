#include "openupdater/core/universal.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace openupdater;

    assert(std::string(update_category_name(UpdateCategory::Application)) ==
           "Applications");
    assert(std::string(update_category_name(UpdateCategory::Programming)) ==
           "Programming");
    assert(std::string(update_category_name(UpdateCategory::Driver)) ==
           "Drivers");

    assert(std::string(update_source_name(UpdateSource::WinGet)) == "WinGet");
    assert(std::string(update_source_name(UpdateSource::WindowsUpdate)) ==
           "Windows Update");

    DiscoveredUpdate update;
    update.id = "Microsoft.VisualStudioCode";
    update.name = "Visual Studio Code";
    update.current_version = "1.105.0";
    update.available_version = "1.106.0";
    update.category = UpdateCategory::Programming;
    update.source = UpdateSource::WinGet;
    update.provider = "WinGet";

    assert(update.id == "Microsoft.VisualStudioCode");
    assert(update.category == UpdateCategory::Programming);
    assert(update.source == UpdateSource::WinGet);

    UniversalUpdateManager manager;
#ifndef _WIN32
    const auto result = manager.check_all();
    assert(result.updates.empty());
#endif

    return 0;
}
