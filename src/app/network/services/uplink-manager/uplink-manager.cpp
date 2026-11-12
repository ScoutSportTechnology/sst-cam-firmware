#include "app/network/services/uplink-manager/uplink-manager.hpp"

namespace sst::network {

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
