#pragma once

#include <stdexcept>
#include <string>

namespace openupdater {

enum class ErrorCode {
    InvalidArgument,
    InvalidVersion,
    UpdateCheckFailed,
    DownloadFailed,
    VerificationFailed,
    InstallationFailed,
    RollbackFailed,
    DependencyFailed,
    TransactionFailed
};

class UpdateError final : public std::runtime_error {
public:
    UpdateError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

} // namespace openupdater
