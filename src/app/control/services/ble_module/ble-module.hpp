#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "app/control/ports/ble-transport.hpp"
#include "app/control/ports/controller.hpp"

namespace sst::control {

// Owns the BLE transport and a registry of controllers keyed by IController::Id().
// On each incoming Command from the transport, dispatches to the controller
// whose Id() matches Command::route, then sends the CommandResult back.
class BleModule {
   public:
    explicit BleModule(std::unique_ptr<IBleTransport> transport);
    ~BleModule();

    BleModule(const BleModule&) = delete;
    auto operator=(const BleModule&) -> BleModule& = delete;
    BleModule(BleModule&&) = delete;
    auto operator=(BleModule&&) -> BleModule& = delete;

    auto Register(std::shared_ptr<IController> controller) -> void;

    auto Start() -> void;
    auto Stop() -> void;
    auto IsRunning() const -> bool;

   private:
    auto OnCommand(Command cmd) -> void;

    std::unique_ptr<IBleTransport> transport_;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<IController>> controllers_;
};

}  // namespace sst::control
