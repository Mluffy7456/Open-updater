#pragma once

#include "openupdater/core/version.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace openupdater {

struct ComponentDependency {
    std::string component;
    Version minimum_version;
};

struct ComponentMetadata {
    std::string component;
    Version version;
    std::vector<ComponentDependency> dependencies;
};

struct Manifest {
    std::string application;
    Version version;
    std::string package;
    std::vector<ComponentMetadata> components;
};

[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);

} // namespace openupdater
