#pragma once

#include <optional>
#include <string>

#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace sst::network {

// Result of applying the Ethernet interface (static via `ip`, or DHCP via
// `dhclient`). `detail` carries the bound address (CIDR) on success, or the
// failure reason. Distinct from the WiFi UplinkResult so the Ethernet-owning
// adapter can grow independently of the WiFi-STA/GO configurator.
struct EthernetApplyResult {
    bool ok{false};
    std::string detail;
};

// The REAL, ground-truth Ethernet state read back from the interface the
// firmware owns. `mode` is derived from reality (address bound + dhclient lease
// presence + the mode the firmware last applied), NOT a default — this is what
// dissolves the DHCP-when-static reporting bug (R12). `address` is the current
// CIDR (empty when down); `carrier` is the physical link state; `gateway` is the
// default gateway on this interface when known (empty otherwise).
struct EthernetProbe {
    EthernetMode mode{EthernetMode::kDown};
    std::string address;
    bool carrier{false};
    std::string gateway;
};

// Port for the firmware-OWNED Ethernet interface. Unlike WiFi (NetworkManager /
// wpa_supplicant coordinated), the firmware runs as root and drives Ethernet
// directly with `ip`/`dhclient` (NM carve-out, U2), so it is the single source
// of truth for the Ethernet config and can report the mode it actually set.
// Hardware-bound: the real adapter forks `ip`/`dhclient` and is verified
// on-device.
class IEthernetController {
   public:
    IEthernetController() = default;
    virtual ~IEthernetController() = default;

    IEthernetController(const IEthernetController&) = delete;
    auto operator=(const IEthernetController&) -> IEthernetController& = delete;
    IEthernetController(IEthernetController&&) = delete;
    auto operator=(IEthernetController&&) -> IEthernetController& = delete;

    // Apply the Ethernet config: static → `ip addr replace` + `ip route replace
    // default via <gw>` + write /etc/resolv.conf; DHCP → `dhclient -1 -4 <if>`
    // (root, default lease/pid paths; the dhclient-script writes resolv.conf).
    // Idempotent / self-healing from a dirty prior state. Records the mode it set
    // so a later ProbeEthernet reports reality. BLE-supplied address/gateway/DNS
    // are validated as strict IPv4 literals BEFORE reaching any argv or the
    // resolv.conf writer (injection + content safety).
    virtual auto ApplyEthernet(const sst::config::EthernetUplink& cfg) -> EthernetApplyResult = 0;

    // Probe the LIVE Ethernet state (reality, not the last-apply return value):
    // carrier, bound address, real static-vs-DHCP mode, gateway. Tolerates a
    // carrier-down interface (returns kDown, no address, never throws).
    virtual auto ProbeEthernet() -> EthernetProbe = 0;

    // Acquire a DHCP OFFER for the Ethernet interface WITHOUT binding the address
    // — so the arbiter (U4) can classify same-vs-different subnet BEFORE it
    // commits an address (the same-subnet dual-home window). Returns the offered
    // address as CIDR, or nullopt when no offer arrives within the bound. See the
    // adapter for the exact ISC-dhclient mechanic (a no-op `-sf` script so the
    // offer is recorded to a scratch leases file but never applied to the iface).
    virtual auto ProbeDhcpOffer() -> std::optional<std::string> = 0;
};

}  // namespace sst::network
