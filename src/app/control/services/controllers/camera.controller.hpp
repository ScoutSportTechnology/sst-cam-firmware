#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"

namespace sst::control {

// Skeleton — fills in once the .proto schema for camera commands is shared.
// Will route verbs like "get_config" / "set_exposure" / "calibrate".
class CameraController final : public IController {
   public:
    CameraController() = default;

    auto Id() const -> std::string_view override { return "camera"; }
    auto Handle(const Command& cmd) -> CommandResult override;
};

}  // namespace sst::control
