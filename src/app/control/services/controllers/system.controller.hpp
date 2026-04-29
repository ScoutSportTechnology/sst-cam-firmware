#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"

namespace sst::control {

// Skeleton — device info, reboot, version, time-sync verbs will land here.
class SystemController final : public IController {
   public:
    SystemController() = default;

    auto Id() const -> std::string_view override { return "system"; }
    auto Handle(const Command& cmd) -> CommandResult override;
};

}  // namespace sst::control
