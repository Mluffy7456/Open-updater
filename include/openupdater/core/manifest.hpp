#pragma once

#include "openupdater/core/version.hpp"

#include <filesystem>
#include <string>

namespace openupdater {

struct Manifest {
    std::string application;
    Version version;
    std::string package;
};

[[nodiscard]] Manifest load_manifest(const std::filesystem::path& path);

} // namespace openupdater
