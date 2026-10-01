#pragma once

#include "openupdater/core/manifest.hpp"

#include <filesystem>

namespace openupdater {

enum class UpdateState {
    UpToDate,
    UpdateAvailable
};

struct UpdateCheck {
    Version current;
    Version available;
    UpdateState state;
};

class Updater {
public:
    [[nodiscard]] static UpdateCheck check(
        const Version& current,
        const Manifest& manifest);

    static void install(
        const std::filesystem::path& package,
        const std::filesystem::path& destination);
};

} // namespace openupdater
