#include "openupdater/core/manifest.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace openupdater {

namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;

    while (std::getline(stream, item, separator)) {
        item = trim(item);
        if (!item.empty())
            result.push_back(item);
    }

    return result;
}

ComponentDependency parse_dependency(const std::string& value) {
    const auto separator = value.find(">=");
    if (separator == std::string::npos)
        throw std::runtime_error(
            "Invalid dependency, expected component>=version: " + value);

    ComponentDependency dependency{
        trim(value.substr(0, separator)),
        Version(trim(value.substr(separator + 2)))
    };

    if (dependency.component.empty() || !dependency.minimum_version.valid())
        throw std::runtime_error("Invalid dependency: " + value);

    return dependency;
}

} // namespace

Manifest load_manifest(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Cannot open manifest: " + path.string());

    Manifest manifest;
    std::string line;
    ComponentMetadata current_component;
    bool in_component = false;

    const auto finish_component = [&]() {
        if (!in_component)
            return;

        if (current_component.component.empty())
            throw std::runtime_error("Manifest component has no name.");

        if (!current_component.version.valid())
            throw std::runtime_error(
                "Manifest component has an invalid version: " +
                current_component.component);

        manifest.components.push_back(current_component);
        current_component = {};
        in_component = false;
    };

    while (std::getline(input, line)) {
        line = trim(line);

        if (line.empty() || line.front() == '#')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            const auto section = trim(
                line.substr(1, line.size() - 2));

            constexpr std::string_view prefix = "component:";
            if (section.rfind(prefix, 0) != 0)
                throw std::runtime_error("Invalid manifest section: " + line);

            finish_component();
            current_component.component =
                trim(section.substr(prefix.size()));

            if (current_component.component.empty())
                throw std::runtime_error(
                    "Manifest component name cannot be empty.");

            in_component = true;
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error("Invalid manifest line: " + line);

        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));

        if (in_component) {
            if (key == "version") {
                current_component.version = Version(value);
            } else if (key == "dependencies") {
                current_component.dependencies.clear();
                for (const auto& dependency : split(value, ',')) {
                    current_component.dependencies.push_back(
                        parse_dependency(dependency));
                }
            } else {
                throw std::runtime_error(
                    "Unknown component manifest key: " + key);
            }
        } else if (key == "application") {
            manifest.application = value;
        } else if (key == "version") {
            manifest.version = Version(value);
        } else if (key == "package") {
            manifest.package = value;
        } else {
            throw std::runtime_error(
                "Unknown manifest key: " + key);
        }
    }

    finish_component();

    if (manifest.application.empty())
        throw std::runtime_error("Manifest is missing 'application'.");
    if (!manifest.version.valid())
        throw std::runtime_error("Manifest contains an invalid 'version'.");
    if (manifest.package.empty())
        throw std::runtime_error("Manifest is missing 'package'.");

    return manifest;
}

} // namespace openupdater
