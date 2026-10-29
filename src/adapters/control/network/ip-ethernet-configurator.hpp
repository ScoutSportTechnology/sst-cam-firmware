#pragma once

#include <optional>
#include <string>

#include "app/network/ports/ethernet-controller.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace sst::adapters::control {

// Firmware-OWNED Ethernet configurator (hardware-bound — fork/exec `ip` +
// `dhclient`). The firmware runs as root and takes Ethernet out of
// NetworkManager (U2 carve-out), so it drives the interface directly rather than
// through nmcli — which dissolves the DHCP-when-static reporting bug by
// construction: the firmware knows the mode it set and reads back the real
// address/lease state (R12). The wired iface is discovered at runtime (mirrors
// deploy/install.sh detect_eth_iface: first ARPHRD_ETHER, non-wireless,
// non-virtual iface under /sys/class/net). Verified on-device.
class IpEthernetConfigurator final : public sst::network::IEthernetController {
   public:
    IpEthernetConfigurator() = default;
    ~IpEthernetConfigurator() override = default;

    IpEthernetConfigurator(const IpEthernetConfigurator&) = delete;
    auto operator=(const IpEthernetConfigurator&) -> IpEthernetConfigurator& = delete;
    IpEthernetConfigurator(IpEthernetConfigurator&&) = delete;
    auto operator=(IpEthernetConfigurator&&) -> IpEthernetConfigurator& = delete;

    auto ApplyEthernet(const sst::config::EthernetUplink& cfg)
        -> sst::network::EthernetApplyResult override;
    auto ProbeEthernet() -> sst::network::EthernetProbe override;
    auto TeardownEthernet() -> void override;
    auto ProbeDhcpOffer() -> std::optional<std::string> override;

   private:
    // Static assignment: validate + `ip addr replace` + default route + resolv.conf.
    auto ApplyStatic(const std::string& iface,
                     const sst::config::EthernetUplink& cfg) -> sst::network::EthernetApplyResult;
    // DHCP: flush stale static state, then `dhclient -1 -4 <iface>` (bounded).
    auto ApplyDhcp(const std::string& iface) -> sst::network::EthernetApplyResult;

    // The last mode the firmware itself applied — the source of truth for
    // readback (cross-checked against live lease/address state in ProbeEthernet).
    sst::network::EthernetMode last_mode_{sst::network::EthernetMode::kDown};

    // PROVENANCE (metal finding): the exact CIDR THIS firmware put on the wired NIC,
    // empty when it has applied none. Teardown removes only this address — never a
    // blanket `ip addr flush`. Rationale: the firmware owns the NIC (R1) but the
    // address on it may predate that ownership (NetworkManager, or an operator's
    // static management IP). Flushing such an address on any carrier blip or
    // de-selection strands the camera off the LAN with no route back — observed on
    // metal, where a cable replug wiped a 10.10.1.30 the firmware never set and only
    // a BLE re-provision recovered it. "Only destroy what you created."
    std::string applied_address_;
};

}  // namespace sst::adapters::control
