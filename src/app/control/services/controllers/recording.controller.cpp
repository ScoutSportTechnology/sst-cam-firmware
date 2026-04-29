#include "app/control/services/controllers/recording.controller.hpp"

#include <spdlog/spdlog.h>

namespace sst::control {

auto RecordingController::Handle(const Command& cmd) -> CommandResult {
    spdlog::warn("RecordingController: not implemented (route=\"{}\", payload {} bytes)",
                 cmd.route, cmd.payload.size());
    CommandResult result;
    result.status = CommandStatus::kUnimplemented;
    return result;
}

}  // namespace sst::control
