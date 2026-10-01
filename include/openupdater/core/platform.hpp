#pragma once

#include <string>
#include <string_view>

namespace openupdater {

enum class Platform {
    Windows,
    Linux,
    MacOS,
    Unknown
};

enum class Architecture {
    X64,
    X86,
    Arm64,
    Arm32,
    Unknown
};

[[nodiscard]] Platform current_platform() noexcept;
[[nodiscard]] Architecture current_architecture() noexcept;

[[nodiscard]] std::string_view platform_name(Platform platform) noexcept;
[[nodiscard]] std::string_view architecture_name(Architecture architecture) noexcept;

// OpenUpdater asset naming convention:
//   <component>-<platform>-<architecture>.<extension>
// Examples:
//   DemoApp-windows-x64.zip
//   DemoApp-linux-arm64.tar.gz
//   DemoApp-macos-arm64.zip
//
// The matcher is intentionally independent of GitHub so it can be tested
// without network access.
[[nodiscard]] bool asset_matches_platform(
    std::string_view asset_name,
    Platform platform,
    Architecture architecture) noexcept;

} // namespace openupdater
