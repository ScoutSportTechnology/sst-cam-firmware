#include "app/network/services/uplink-manager/uplink-manager.hpp"

#include <spdlog/spdlog.h>

namespace sst::network {

auto UplinkManager::Apply(const sst::config::UplinkData& cfg) -> UplinkStatus {
    UplinkStatus status;

    status.ethernet_enabled = cfg.ethernet.enabled.value_or(false);
    if (status.ethernet_enabled) {
        const auto applied = ethernet_.ApplyEthernet(cfg.ethernet);
        status.ethernet = {.ok = applied.ok, .detail = applied.detail};
        spdlog::info("UplinkManager: ethernet uplink {} ({})", status.ethernet.ok ? "up" : "FAILED",
                     status.ethernet.detail);
    } else {
        spdlog::info("UplinkManager: ethernet uplink disabled — skipped");
    }

    status.wifi_enabled = cfg.wifi.enabled.value_or(false);
    if (status.wifi_enabled) {
        status.wifi = configurator_.ApplyWifiSta(cfg.wifi);
        spdlog::info("UplinkManager: wifi-STA uplink {} ({})",
                     status.wifi.ok ? "up" : "unavailable", status.wifi.detail);
    } else {
        spdlog::info("UplinkManager: wifi-STA uplink disabled — skipped");
    }

    return status;
}

auto UplinkManager::QueryStatus() -> UplinkStatus {
    UplinkStatus status;
    // Ethernet: reality from the firmware-owned controller. up == a usable mode
    // (not kDown); detail == the real CIDR. The handler reads the richer
    // EthernetProbe directly for the static-vs-DHCP mode (R12).
    const auto eth = ethernet_.ProbeEthernet();
    status.ethernet = {.ok = eth.mode != sst::network::EthernetMode::kDown, .detail = eth.address};
    status.ethernet_enabled = status.ethernet.ok;
    status.wifi = configurator_.ProbeWifiSta();
    status.wifi_enabled = status.wifi.ok;
    return status;
}

}  // namespace sst::network
