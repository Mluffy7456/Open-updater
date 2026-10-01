#pragma once

#include "openupdater/core/universal.hpp"

namespace openupdater {

class WinGetProvider final : public IUpdateProvider {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool available() const override;
    [[nodiscard]] std::vector<DiscoveredUpdate> check_updates() override;
    void install(const DiscoveredUpdate& update) override;
};

} // namespace openupdater
