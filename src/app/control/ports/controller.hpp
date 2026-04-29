#pragma once

#include <string_view>

#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"

namespace sst::control {

// One per controllable concern (camera, recording, streaming, network, ...).
// Drop-in extension point: a new xxx.controller.{hpp,cpp} implements IController
// and is registered with BleModule via a single Register() line in main.cpp.
class IController {
   public:
    virtual ~IController() = default;
    virtual auto Id() const -> std::string_view = 0;
    virtual auto Handle(const Command& cmd) -> CommandResult = 0;
};

}  // namespace sst::control
