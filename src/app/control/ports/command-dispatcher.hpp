#pragma once

#include "bluetooth.pb.h"

namespace sst::control {

// Single entry point from the BLE transport into the control plane. Given a
// decoded proto Command, returns exactly one CommandResponse with a matching
// correlation_id — always, for every command (R4). Unknown / unwired commands
// return status=UNSUPPORTED (R6); recognized-but-failing commands return
// status=ERROR with an error_message.
class ICommandDispatcher {
   public:
    virtual ~ICommandDispatcher() = default;

    virtual auto Dispatch(const sst_cam::Command& cmd) -> sst_cam::CommandResponse = 0;
};

}  // namespace sst::control
