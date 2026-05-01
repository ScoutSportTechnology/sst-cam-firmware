#pragma once

#include <cstdint>
#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/control/services/wifi_module/wifi-module.hpp"
#include "app/db/ports/network-repository.hpp"

namespace sst::control {

// Routes "network.*" commands. The BLE-bootstraps-WiFi flow lives here:
//   1. Phone connects over BLE.
//   2. Phone sends Command{ route="network", payload="bootstrap_p2p" }.
//   3. NetworkController loads the WiFi-Direct credentials from the DB
//      (config_wifi_direct, seeded from config/wifi-direct.json on first
//      boot) and calls WifiModule::StartP2pGroupOwner(...).
//   4. CommandResult payload returns "<ssid>\n<passphrase>" so the phone can
//      auto-join the WiFi-Direct group.
//
// The payload format is provisional — once the .proto schema is fixed the
// payload becomes a typed `BootstrapP2pResponse` message.
class NetworkController final : public IController {
   public:
    NetworkController(WifiModule& wifi, sst::db::INetworkRepository& network_repo,
                      int64_t user_id);

    auto Id() const -> std::string_view override { return "network"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    WifiModule& wifi_;
    sst::db::INetworkRepository& network_repo_;
    int64_t user_id_;
};

}  // namespace sst::control
