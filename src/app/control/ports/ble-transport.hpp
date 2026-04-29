#pragma once

#include <functional>

#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"

namespace sst::control {

// Peripheral-side BLE transport. The adapter advertises a GATT service, exposes
// a "command" characteristic the phone writes to, and a "response"
// characteristic that emits notifications back. Whenever a Command is parsed
// from a write, the adapter invokes OnCommand. SendResult notifies the phone
// of the outcome on the response characteristic.
class IBleTransport {
   public:
    using CommandHandler = std::function<void(Command)>;

    virtual ~IBleTransport() = default;

    virtual auto Start() -> void = 0;
    virtual auto Stop() -> void = 0;
    virtual auto IsRunning() const -> bool = 0;

    virtual auto SetOnCommand(CommandHandler handler) -> void = 0;
    virtual auto SendResult(const CommandResult& result) -> void = 0;
};

}  // namespace sst::control
