#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"

namespace sst::control {

// Skeleton — local recording start/stop will go here once the .proto arrives.
class RecordingController final : public IController {
   public:
    RecordingController() = default;

    auto Id() const -> std::string_view override { return "recording"; }
    auto Handle(const Command& cmd) -> CommandResult override;
};

}  // namespace sst::control
