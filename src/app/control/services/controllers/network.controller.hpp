#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/control/services/wifi_module/wifi-module.hpp"

namespace sst::control {

// Routes "network.*" commands. The BLE-bootstraps-WiFi flow lives here:
//   1. Phone connects over BLE.
//   2. Phone sends Command{ route="network", payload="bootstrap_p2p" }.
//   3. NetworkController calls WifiModule::StartP2pGroupOwner(...) with the
//      hardcoded BootstrapDefaults credentials.
//   4. CommandResult payload returns "<ssid>\n<passphrase>" so the phone can
//      auto-join the WiFi-Direct group.
//
// The payload format is provisional — once the .proto schema is fixed the
// payload becomes a typed `BootstrapP2pResponse` message.
class NetworkController final : public IController {
   public:
    explicit NetworkController(WifiModule& wifi);

    auto Id() const -> std::string_view override { return "network"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    WifiModule& wifi_;
};

}  // namespace sst::control
