#pragma once

#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "domain/config/models/uplink-config.hpp"

namespace sst::network {

// Live uplink status reported to the app.
struct UplinkStatus {
    bool ethernet_enabled{false};
    UplinkResult ethernet;
    bool wifi_enabled{false};
    UplinkResult wifi;
};

// Queries live uplink status for the BLE handler. Boot and runtime binding are
// owned by ConnectivityArbiter.
class UplinkManager {
   public:
    UplinkManager(IUplinkConfigurator& configurator, IEthernetController& ethernet)
        : configurator_(configurator), ethernet_(ethernet) {}

    // Probe the LIVE per-interface state — what
    // GetNetworkConfig reports so the app shows the camera's actual ethernet/wifi.
    auto QueryStatus() -> UplinkStatus;

   private:
    IUplinkConfigurator& configurator_;
    IEthernetController& ethernet_;
};

}  // namespace sst::network
