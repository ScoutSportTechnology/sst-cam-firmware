#pragma once

#include <memory>
#include <unordered_map>

#include "app/control/ports/command-dispatcher.hpp"
#include "app/control/ports/handler.hpp"
#include "bluetooth.pb.h"

namespace sst::control {

// Always-respond proto dispatcher (KTD3). Switches on Command::payload_case()
// and routes to the registered ICommandHandler. A command whose capability
// isn't wired returns status=UNSUPPORTED; a handler that throws returns
// status=ERROR + error_message. Every path emits exactly one CommandResponse
// whose correlation_id echoes the inbound command.
class CommandDispatcher final : public ICommandDispatcher {
   public:
    // Registers a handler for every payload case it declares via HandledCases().
    // A case already owned by another handler is ignored (logged) — one owner
    // per case keeps the routing unambiguous.
    auto Register(std::shared_ptr<ICommandHandler> handler) -> void;

    auto Dispatch(const sst_cam::Command& cmd) -> sst_cam::CommandResponse override;

   private:
    // Keyed by the integer value of Command::PayloadCase.
    std::unordered_map<int, std::shared_ptr<ICommandHandler>> handlers_;
};

}  // namespace sst::control
