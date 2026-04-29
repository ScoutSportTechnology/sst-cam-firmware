#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <sdbus-c++/sdbus-c++.h>

#include "adapters/control/ble/bluez/gatt-application.hpp"
#include "app/control/ports/ble-transport.hpp"

namespace sst::adapters::control {

// BlueZ-via-D-Bus BLE peripheral transport. On Start():
//   1. Connect to the system bus.
//   2. Build a GattApplication exposing one service + command/response chars.
//   3. Register an LEAdvertisement1 object so BlueZ broadcasts the device name.
//   4. Call RegisterAdvertisement and RegisterApplication on org.bluez/<adapter>.
//   5. Start the connection's async event loop on an internal thread.
//
// On Stop(): unregister advertisement + application, leave the event loop, and
// drop all D-Bus objects.
class BluezBleTransport final : public sst::control::IBleTransport {
   public:
    explicit BluezBleTransport(std::string adapter_path = "/org/bluez/hci0");
    ~BluezBleTransport() override;

    BluezBleTransport(const BluezBleTransport&) = delete;
    auto operator=(const BluezBleTransport&) -> BluezBleTransport& = delete;
    BluezBleTransport(BluezBleTransport&&) = delete;
    auto operator=(BluezBleTransport&&) -> BluezBleTransport& = delete;

    auto Start() -> void override;
    auto Stop() -> void override;
    [[nodiscard]] auto IsRunning() const -> bool override;

    auto SetOnCommand(CommandHandler handler) -> void override;
    auto SendResult(const sst::control::CommandResult& result) -> void override;

   private:
    auto BuildAdvertisement() -> void;
    auto OnRawCommand(std::vector<std::uint8_t> bytes) -> void;

    std::string adapter_path_;
    std::string app_root_path_;
    std::string adv_path_;

    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<sdbus::IObject> adv_obj_;
    std::unique_ptr<GattApplication> gatt_app_;

    mutable std::mutex mtx_;
    CommandHandler on_command_;
    std::atomic<bool> running_{false};
    bool advertisement_registered_{false};
    bool application_registered_{false};
};

}  // namespace sst::adapters::control
