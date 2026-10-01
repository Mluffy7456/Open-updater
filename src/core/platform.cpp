#include "openupdater/core/platform.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace openupdater {

Platform current_platform() noexcept {
#ifdef _WIN32
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::MacOS;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

Architecture current_architecture() noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return Architecture::X64;
#elif defined(_M_IX86) || defined(__i386__)
    return Architecture::X86;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return Architecture::Arm64;
#elif defined(_M_ARM) || defined(__arm__)
    return Architecture::Arm32;
#else
    return Architecture::Unknown;
#endif
}

std::string_view platform_name(Platform platform) noexcept {
    switch (platform) {
    case Platform::Windows: return "windows";
    case Platform::Linux: return "linux";
    case Platform::MacOS: return "macos";
    case Platform::Unknown: return "unknown";
    }
    return "unknown";
}

std::string_view architecture_name(Architecture architecture) noexcept {
    switch (architecture) {
    case Architecture::X64: return "x64";
    case Architecture::X86: return "x86";
    case Architecture::Arm64: return "arm64";
    case Architecture::Arm32: return "arm32";
    case Architecture::Unknown: return "unknown";
    }
    return "unknown";
}

namespace {

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool has_token(const std::string& name, std::string_view token) {
    const std::string value = lower(name);
    const std::string wanted = lower(token);

    std::size_t position = 0;
    while ((position = value.find(wanted, position)) != std::string::npos) {
        const bool left_ok =
            position == 0 ||
            !std::isalnum(static_cast<unsigned char>(value[position - 1]));
        const auto end = position + wanted.size();
        const bool right_ok =
            end >= value.size() ||
            !std::isalnum(static_cast<unsigned char>(value[end]));

        if (left_ok && right_ok)
            return true;

        position = end;
    }

    return false;
}

} // namespace

bool asset_matches_platform(
    std::string_view asset_name,
    Platform platform,
    Architecture architecture) {

    if (platform == Platform::Unknown || architecture == Architecture::Unknown)
        return false;

    return has_token(
               std::string(asset_name),
               platform_name(platform)) &&
           has_token(
               std::string(asset_name),
               architecture_name(architecture));
}

} // namespace openupdater
