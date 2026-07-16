#pragma once

#include <string>

#include "app/network/ports/uplink-configurator.hpp"

namespace sst::adapters::control {

// WiFi-STA uplink configurator (hardware-bound — fork/exec `wpa_cli`/`dhclient`/
// `ip`). Ethernet is NO LONGER handled here — it moved to the firmware-owned
// IpEthernetConfigurator (ip/dhclient, NM carve-out) so the real static-vs-DHCP
// mode is reported (R12). This adapter owns only the WiFi-STA join: the radio is
// NM-unmanaged (dedicated to the firmware-owned wpa_supplicant for the
// WiFi-Direct GO), so the STA join runs through that same wpa_supplicant + a
// dhclient lease, not NetworkManager. Verified on-device.
class NmcliUplinkConfigurator final : public sst::network::IUplinkConfigurator {
   public:
    NmcliUplinkConfigurator() = default;
    ~NmcliUplinkConfigurator() override = default;

    NmcliUplinkConfigurator(const NmcliUplinkConfigurator&) = delete;
    auto operator=(const NmcliUplinkConfigurator&) -> NmcliUplinkConfigurator& = delete;
    NmcliUplinkConfigurator(NmcliUplinkConfigurator&&) = delete;
    auto operator=(NmcliUplinkConfigurator&&) -> NmcliUplinkConfigurator& = delete;

    auto ApplyWifiSta(const sst::config::WifiStaUplink& cfg) -> sst::network::UplinkResult override;
    auto TeardownWifiSta() -> void override;
    auto ProbeWifiSta() -> sst::network::UplinkResult override;

   private:
    // The base WiFi station device (TYPE=="wifi", i.e. wlP1p1s0 — NOT a
    // "wifi-p2p" virtual iface), empty if none. Runs
    // `nmcli -t -f DEVICE,TYPE device status`.
    static auto DetectWifiDevice() -> std::string;
};

}  // namespace sst::adapters::control
