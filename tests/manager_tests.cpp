#include "openupdater/core/error.hpp"
#include "openupdater/core/updater.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

openupdater::AvailableUpdate update(
    const std::string& component,
    const std::string& current,
    const std::string& available) {
    return {
        component,
        openupdater::Version(current),
        openupdater::Version(available),
        component + "-windows-x64.zip",
        "windows",
        "x64",
        "digest-" + component,
        "",
        "https://example.invalid/" + component + ".zip"
    };
}

openupdater::ComponentMetadata metadata(
    const std::string& component,
    const std::string& version,
    std::vector<openupdater::ComponentDependency> dependencies = {}) {
    return {component, openupdater::Version(version), std::move(dependencies)};
}

} // namespace

int main() {
    using namespace openupdater;

    static_assert(API_VERSION_MAJOR == 1);
    assert(UpdateError(ErrorCode::DependencyFailed, "dependency").code() ==
           ErrorCode::DependencyFailed);

    const auto core = update("Core", "1.0.0", "2.0.0");
    const auto runtime = update("Runtime", "1.0.0", "2.1.0");

    UpdateManagerRequest request{
        {core, runtime},
        {},
        {
            metadata("Core", "2.0.0", {
                {"Runtime", Version("2.0.0")}
            }),
            metadata("Runtime", "2.1.0")
        },
        {
            {"Core", Version("1.0.0")},
            {"Runtime", Version("1.0.0")}
        },
        "./manager-test-destination",
        {}
    };

    request.selection.select("Core");

    const auto plan = Updater::build_plan(request);

    assert(plan.install_order.size() == 2);
    assert(plan.install_order[0] == "Runtime");
    assert(plan.install_order[1] == "Core");
    assert(plan.updates.size() == 2);
    assert(plan.updates[0].component == "Runtime");
    assert(plan.updates[1].component == "Core");

    request.selection.select("Runtime");
    const auto selected_plan = Updater::build_plan(request);
    assert(selected_plan.install_order.size() == 2);

    UpdateManagerRequest satisfied_dependency{
        {core},
        {},
        {
            metadata("Core", "2.0.0", {
                {"Runtime", Version("2.0.0")}
            })
        },
        {
            {"Core", Version("1.0.0")},
            {"Runtime", Version("2.3.0")}
        },
        "./manager-test-destination",
        {}
    };
    satisfied_dependency.selection.select("Core");

    const auto satisfied_plan = Updater::build_plan(satisfied_dependency);
    assert(satisfied_plan.install_order.size() == 1);
    assert(satisfied_plan.install_order[0] == "Core");

    UpdateManagerRequest unsatisfied{
        {core},
        {},
        {
            metadata("Core", "2.0.0", {
                {"Runtime", Version("2.0.0")}
            })
        },
        {
            {"Core", Version("1.0.0")},
            {"Runtime", Version("1.5.0")}
        },
        "./manager-test-destination",
        {}
    };
    unsatisfied.selection.select("Core");

    bool caught = false;
    try {
        (void)Updater::build_plan(unsatisfied);
    } catch (const UpdateError& error) {
        caught = true;
        assert(error.code() == ErrorCode::DependencyFailed);
    }
    assert(caught);

    const auto a = update("A", "1.0.0", "2.0.0");
    const auto b = update("B", "1.0.0", "2.0.0");

    UpdateManagerRequest cyclic{
        {a, b},
        {},
        {
            metadata("A", "2.0.0", {
                {"B", Version("2.0.0")}
            }),
            metadata("B", "2.0.0", {
                {"A", Version("2.0.0")}
            })
        },
        {
            {"A", Version("1.0.0")},
            {"B", Version("1.0.0")}
        },
        "./manager-test-destination",
        {}
    };
    cyclic.selection.select("A");

    caught = false;
    try {
        (void)Updater::build_plan(cyclic);
    } catch (const UpdateError& error) {
        caught = true;
        assert(error.code() == ErrorCode::DependencyFailed);
    }
    assert(caught);

    const auto manifest_path =
        std::filesystem::temp_directory_path() /
        "openupdater-v2-manager.manifest";

    {
        std::ofstream output(manifest_path);
        assert(output);
        output
            << "application=DemoApp\n"
            << "version=2.0.0\n"
            << "package=DemoApp-2.0.0.zip\n"
            << "\n"
            << "[component:Runtime]\n"
            << "version=2.1.0\n"
            << "dependencies=\n"
            << "\n"
            << "[component:Core]\n"
            << "version=2.0.0\n"
            << "dependencies=Runtime>=2.0.0\n";
    }

    const auto manifest = load_manifest(manifest_path);
    assert(manifest.components.size() == 2);
    assert(manifest.components[0].component == "Runtime");
    assert(manifest.components[1].component == "Core");
    assert(manifest.components[1].dependencies.size() == 1);
    assert(manifest.components[1].dependencies[0].component == "Runtime");
    assert(
        manifest.components[1].dependencies[0].minimum_version ==
        Version("2.0.0"));

    std::filesystem::remove(manifest_path);

    return 0;
}
