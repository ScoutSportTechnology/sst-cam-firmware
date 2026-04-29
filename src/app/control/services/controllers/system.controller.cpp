#include "app/control/services/controllers/system.controller.hpp"

#include <spdlog/spdlog.h>

namespace sst::control {

auto SystemController::Handle(const Command& cmd) -> CommandResult {
    spdlog::warn("SystemController: not implemented (route=\"{}\", payload {} bytes)",
                 cmd.route, cmd.payload.size());
    CommandResult result;
    result.status = CommandStatus::kUnimplemented;
    return result;
}

}  // namespace sst::control
