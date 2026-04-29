#include "app/control/services/controllers/camera.controller.hpp"

#include <spdlog/spdlog.h>

namespace sst::control {

auto CameraController::Handle(const Command& cmd) -> CommandResult {
    spdlog::warn("CameraController: not implemented (route=\"{}\", payload {} bytes)",
                 cmd.route, cmd.payload.size());
    CommandResult result;
    result.status = CommandStatus::kUnimplemented;
    return result;
}

}  // namespace sst::control
