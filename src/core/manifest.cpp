#include "openupdater/core/manifest.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace openupdater {

namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

Manifest load_manifest(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Cannot open manifest: " + path.string());

    Manifest manifest;
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);

        if (line.empty() || line.front() == '#')
            continue;

        const auto separator = line.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error("Invalid manifest line: " + line);

        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));

        if (key == "application")
            manifest.application = value;
        else if (key == "version")
            manifest.version = Version(value);
        else if (key == "package")
            manifest.package = value;
    }

    if (manifest.application.empty())
        throw std::runtime_error("Manifest is missing 'application'.");
    if (!manifest.version.valid())
        throw std::runtime_error("Manifest contains an invalid 'version'.");
    if (manifest.package.empty())
        throw std::runtime_error("Manifest is missing 'package'.");

    return manifest;
}

} // namespace openupdater
