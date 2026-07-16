#pragma once

#include <string>

namespace sst::network {

// The routing facts for the ONE legitimate concurrent dual-interface config (R4):
// Ethernet on a DIFFERENT subnet as an internet uplink while WiFi-STA carries the
// LAN. The arbiter derives these from its firmware-owned Ethernet probe and hands
// them to the policy-routing collaborator; the adapter resolves the wired
// interface name itself. All fields are ground-truth IPv4 literals/CIDRs from the
// live interface (the adapter re-validates before any `ip`/`nft` argv).
struct UplinkRoutingConfig {
    std::string uplink_address_cidr;  // the uplink's bound address WITH prefix, "192.168.9.30/24"
    std::string uplink_gateway;       // the default gateway on the uplink, "192.168.9.1" (may be
                                      // empty when unknown — then no default route is installed)

    auto operator==(const UplinkRoutingConfig&) const -> bool = default;
};

// Applies (and tears down) source-based policy routing + an egress-only firewall
// for the different-subnet Ethernet uplink (R4). This is the SAME source-routing
// mechanics as the retired same-subnet table-142 workaround (U5 deleted that),
// now CORRECTLY applied to genuinely-different subnets: a dedicated firmware-
// private routing table + `ip rule` steers uplink-sourced egress out the uplink
// while LAN-sourced traffic keeps the main table — neither interface steals the
// other's traffic. The uplink is firewalled EGRESS-ONLY (default-deny inbound
// except established/related) so no camera service is exposed on it: THIS is the
// security bound that lets the firmware run as root (B1). Every method is
// idempotent / self-healing (replace-or-flush-then-add) so a re-apply from a dirty
// prior state converges.
//
// Hardware-bound: the real adapter forks `ip`/`nft` (root, CAP_NET_ADMIN) and is
// verified on-device; a test double records the calls.
class IPolicyRouting {
   public:
    IPolicyRouting() = default;
    virtual ~IPolicyRouting() = default;

    IPolicyRouting(const IPolicyRouting&) = delete;
    auto operator=(const IPolicyRouting&) -> IPolicyRouting& = delete;
    IPolicyRouting(IPolicyRouting&&) = delete;
    auto operator=(IPolicyRouting&&) -> IPolicyRouting& = delete;

    // Install the uplink's dedicated table + `ip rule` + egress-only firewall.
    // Idempotent: a re-apply flushes-then-re-adds so it converges regardless of
    // prior state. Returns false if the wired interface could not be resolved or a
    // critical `ip` step failed (the arbiter logs + keeps STA as the LAN either
    // way; a failed uplink apply never wedges the LAN path).
    virtual auto ApplyUplinkRouting(const UplinkRoutingConfig& cfg) -> bool = 0;

    // Remove the uplink table, `ip rule`, and firewall (best-effort, log-and-
    // continue). Idempotent — safe when nothing was applied. Called when Ethernet
    // is no longer a different-subnet uplink (unplug / became same-subnet LAN /
    // full revert).
    virtual auto TeardownUplinkRouting() -> void = 0;
};

}  // namespace sst::network
