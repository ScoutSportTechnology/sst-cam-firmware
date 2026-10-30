#pragma once

#include <string>

#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "domain/config/models/uplink-config.hpp"

namespace sst::network {

// Aggregate status after applying an uplink config — what the GetNetworkConfig
// BLE response (U4) reports back to the app.
struct UplinkStatus {
    bool ethernet_enabled{false};
    UplinkResult ethernet;
    bool wifi_enabled{false};
    UplinkResult wifi;
};

// Orchestrates the camera's internet uplink from its persisted/pushed config:
// brings up each enabled interface via the firmware-owned Ethernet controller
// (ip/dhclient) + the WiFi-STA configurator, independent of the WiFi-Direct GO.
// Applied on boot and whenever the app pushes a new NetworkConfig (U4). Disabled
// interfaces are skipped, so a disabled uplink is simply not driven by us.
class UplinkManager {
   public:
    UplinkManager(IUplinkConfigurator& configurator, IEthernetController& ethernet)
        : configurator_(configurator), ethernet_(ethernet) {}

    // Apply the config; returns the per-interface outcome for status reporting.
    auto Apply(const sst::config::UplinkData& cfg) -> UplinkStatus;

    // Probe the LIVE per-interface state (reality, not the last apply) — what
    // GetNetworkConfig reports so the app shows the camera's actual ethernet/wifi.
    auto QueryStatus() -> UplinkStatus;

   private:
    IUplinkConfigurator& configurator_;
    IEthernetController& ethernet_;
};

}  // namespace sst::network
