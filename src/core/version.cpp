#include "openupdater/core/version.hpp"

#include <charconv>
#include <utility>
#include <vector>

namespace openupdater {

namespace {

std::vector<int> parse(std::string_view value) {
    std::vector<int> parts;

    if (!value.empty() && value.front() == 'v')
        value.remove_prefix(1);

    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find('.', start);
        const auto token = value.substr(
            start,
            end == std::string_view::npos ? value.size() - start : end - start);

        if (token.empty())
            return {};

        int number = 0;
        const auto result = std::from_chars(
            token.data(), token.data() + token.size(), number);

        if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || number < 0)
            return {};

        parts.push_back(number);

        if (end == std::string_view::npos)
            break;

        start = end + 1;
    }

    return parts;
}

} // namespace

Version::Version(std::string value) : value_(std::move(value)) {}

const std::string& Version::str() const noexcept {
    return value_;
}

bool Version::valid() const noexcept {
    return !parse(value_).empty();
}

int compare_versions(std::string_view lhs, std::string_view rhs) {
    const auto left = parse(lhs);
    const auto right = parse(rhs);

    if (left.empty() || right.empty())
        return 0;

    const auto count = left.size() > right.size() ? left.size() : right.size();

    for (std::size_t i = 0; i < count; ++i) {
        const auto a = i < left.size() ? left[i] : 0;
        const auto b = i < right.size() ? right[i] : 0;

        if (a < b) return -1;
        if (a > b) return 1;
    }

    return 0;
}

bool operator==(const Version& lhs, const Version& rhs) {
    return compare_versions(lhs.str(), rhs.str()) == 0;
}

bool operator!=(const Version& lhs, const Version& rhs) {
    return !(lhs == rhs);
}

bool operator<(const Version& lhs, const Version& rhs) {
    return compare_versions(lhs.str(), rhs.str()) < 0;
}

bool operator>(const Version& lhs, const Version& rhs) {
    return rhs < lhs;
}

bool operator<=(const Version& lhs, const Version& rhs) {
    return !(rhs < lhs);
}

bool operator>=(const Version& lhs, const Version& rhs) {
    return !(lhs < rhs);
}

} // namespace openupdater
