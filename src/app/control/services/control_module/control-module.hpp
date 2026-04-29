#pragma once

#include <memory>

#include "app/control/ports/ble-transport.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/control/services/ble_module/ble-module.hpp"
#include "app/control/services/wifi_module/wifi-module.hpp"

namespace sst::control {

// Top-level facade for the control plane. Owns the BLE submodule (commands)
// and the WiFi submodule (network bring-up). Mirrors the DbManager pattern
// from src/app/db/services/db_manager/.
class ControlModule {
   public:
    ControlModule(std::unique_ptr<IBleTransport> ble_transport,
                  std::unique_ptr<IWifiManager> wifi_manager);

    auto Start() -> void;
    auto Stop() -> void;

    auto ble() -> BleModule& { return ble_; }
    auto wifi() -> WifiModule& { return wifi_; }

   private:
    BleModule ble_;
    WifiModule wifi_;
};

}  // namespace sst::control
