#pragma once

#include <string>

#include "domain/config/models/uplink-config.hpp"

namespace sst::network {

// Result of applying one uplink interface.
struct UplinkResult {
    bool ok{false};
    std::string detail;  // address on success, or the failure reason
};

// Port for the camera's WiFi-STA uplink — joining the phone's network as a
// client, SEPARATE from the WiFi-Direct GO. Ethernet is NO LONGER owned here: it
// moved to the firmware-owned IEthernetController (ip/dhclient, NM carve-out) so
// GetNetworkConfig can report the real static-vs-DHCP mode (R12). The STA join is
// driven through the firmware-owned wpa_supplicant + dhclient (the radio is
// NM-unmanaged, dedicated to the WiFi-Direct GO). Hardware-bound; the real
// adapter forks `wpa_cli`/`dhclient`/`ip` and is verified on-device.
class IUplinkConfigurator {
   public:
    IUplinkConfigurator() = default;
    virtual ~IUplinkConfigurator() = default;

    IUplinkConfigurator(const IUplinkConfigurator&) = delete;
    auto operator=(const IUplinkConfigurator&) -> IUplinkConfigurator& = delete;
    IUplinkConfigurator(IUplinkConfigurator&&) = delete;
    auto operator=(IUplinkConfigurator&&) -> IUplinkConfigurator& = delete;

    // Apply the WiFi-STA uplink (join the phone's network as a client). The
    // single-radio GO+STA concurrency hazard is designed out by the connectivity
    // arbiter's INV2 invariant (KTD2/KTD3): the GO is always torn down before this
    // is called, so the adapter is free to drive the STA join.
    virtual auto ApplyWifiSta(const sst::config::WifiStaUplink& cfg) -> UplinkResult = 0;

    // Cleanly tear the WiFi-STA uplink down: release the DHCP lease + kill its
    // dhclient, disconnect the wpa association, and flush the STA address + routes
    // off the radio. Called by the arbiter on EVERY revert to S1 so a leftover STA
    // address/route can never linger and fight another interface (e.g. ethernet on
    // the same subnet) or leave the iface dirty for the WiFi-Direct GO. Idempotent.
    virtual auto TeardownWifiSta() -> void = 0;

    // Probe the LIVE WiFi-STA state (not the last-applied config) so the app's
    // Settings -> Network shows reality. `ok` = associated to a real AP with an
    // address; `detail` = its current address / status.
    virtual auto ProbeWifiSta() -> UplinkResult = 0;
};

}  // namespace sst::network
