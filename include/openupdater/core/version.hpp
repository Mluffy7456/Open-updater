#pragma once

#include <string>
#include <string_view>

namespace openupdater {

class Version {
public:
    Version() = default;
    explicit Version(std::string value);

    [[nodiscard]] const std::string& str() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    friend bool operator==(const Version& lhs, const Version& rhs);
    friend bool operator!=(const Version& lhs, const Version& rhs);
    friend bool operator<(const Version& lhs, const Version& rhs);
    friend bool operator>(const Version& lhs, const Version& rhs);
    friend bool operator<=(const Version& lhs, const Version& rhs);
    friend bool operator>=(const Version& lhs, const Version& rhs);

private:
    std::string value_;
};

[[nodiscard]] int compare_versions(std::string_view lhs, std::string_view rhs);

} // namespace openupdater
