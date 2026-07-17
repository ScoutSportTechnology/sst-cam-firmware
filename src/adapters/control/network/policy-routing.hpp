#pragma once

#include <string>
#include <vector>

#include "app/network/ports/policy-routing.hpp"

namespace sst::adapters::control {

// Firmware-private routing table + `ip rule` priority for the different-subnet
// uplink (R4). DISTINCT from the retired same-subnet table-142 workaround (U5
// deleted that) so an old rule can never be confused with this one. The table
// holds ONLY the uplink's connected + default routes; the `ip rule` selects it
// for traffic SOURCED from the uplink address, so LAN (STA) traffic keeps the
// main table — neither interface steals the other's egress.
inline constexpr const char* kUplinkRouteTable = "210";
inline constexpr const char* kUplinkRulePriority = "2100";
// nftables table (family `inet`) holding the egress-only uplink firewall. Deleting
// the whole table tears the firewall down atomically.
inline constexpr const char* kUplinkNftTable = "sst_uplink";

// True iff `name` is a safe Linux interface name to interpolate into an `ip`/`nft`
// argv: non-empty, <= IFNAMSIZ-1 (15) chars, only [A-Za-z0-9._-], not starting
// with '-'. The wired iface is read from /sys/class/net (not BLE-supplied), but
// every interpolated token is whitelisted regardless (defense-in-depth, the
// injection-safety rule).
auto IsSafeIfaceName(const std::string& name) -> bool;

// The network (subnet) CIDR of a host CIDR: "192.168.9.30/24" -> "192.168.9.0/24".
// nullopt / empty on a malformed input. Used to install the uplink's connected
// route in the dedicated table.
auto NetworkCidrOf(const std::string& host_cidr) -> std::string;

// ── Pure argv builders (exec-free, unit-tested) ──────────────────────────────
// Each validates its interpolated tokens and returns an EMPTY vector on invalid
// input, so the adapter fails fast instead of exec'ing a half-formed — or
// attacker-shaped — command. All argv[0] resolve via PATH (`ip`/`nft`).

// `ip rule add from <uplink_address> table 210 priority 2100`. Empty on a bad IP.
auto BuildUplinkRuleAddArgv(const std::string& uplink_address) -> std::vector<std::string>;
// `ip rule del priority 2100` — removes our rule by priority regardless of the
// source address (idempotent teardown; no stored address needed).
auto BuildUplinkRuleDelArgv() -> std::vector<std::string>;
// `ip route replace <subnet_cidr> dev <iface> table 210` — the uplink's connected
// route in the dedicated table (so the default's `via` resolves). Empty on a bad
// CIDR / iface.
auto BuildUplinkSubnetRouteArgv(const std::string& subnet_cidr,
                                const std::string& iface) -> std::vector<std::string>;
// `ip route replace default via <gateway> dev <iface> table 210`. Empty on a bad
// gateway literal / iface.
auto BuildUplinkDefaultRouteArgv(const std::string& gateway,
                                 const std::string& iface) -> std::vector<std::string>;
// `ip route flush table 210` — drops all uplink routes (idempotent teardown).
auto BuildUplinkRouteFlushArgv() -> std::vector<std::string>;

// Egress-only firewall (nft). The chain hooks `input` with a default ACCEPT
// policy and matches ONLY the uplink iface, so non-uplink (LAN) traffic is never
// touched; on the uplink, established/related return traffic is accepted and
// everything else inbound is dropped — no camera service is reachable on the
// uplink. Building a full multi-step ruleset via argv (not a shell string) keeps
// every interpolated token injection-safe.
auto BuildFirewallAddTableArgv() -> std::vector<std::string>;
auto BuildFirewallFlushTableArgv() -> std::vector<std::string>;
auto BuildFirewallAddChainArgv() -> std::vector<std::string>;
// `nft add rule inet sst_uplink input iifname <iface> ct state established,related accept`.
auto BuildFirewallAllowEstablishedArgv(const std::string& iface) -> std::vector<std::string>;
// `nft add rule inet sst_uplink input iifname <iface> drop` — drop all other
// inbound on the uplink (the egress-only bound). Empty on a bad iface.
auto BuildFirewallDropInboundArgv(const std::string& iface) -> std::vector<std::string>;
// `nft delete table inet sst_uplink` — tears the whole firewall down atomically.
auto BuildFirewallDeleteTableArgv() -> std::vector<std::string>;

// F7: the ENTIRE egress-only firewall as a SINGLE `nft -f` script (add table; flush
// table; add chain; allow established/related; drop all other inbound on <iface>).
// nft applies the whole script as ONE atomic transaction, so a re-apply swaps the
// ruleset with NO rule-less window — unlike the old flush-then-readd sequence of
// separate invocations, which briefly let inbound through on the uplink. Empty on an
// unsafe iface (never emit a script with an un-whitelisted token). The iface is the
// only interpolated value and is IsSafeIfaceName-validated, so the quoted `iifname
// "<iface>"` literal can carry no shell/nft metacharacters, newline, or directive.
auto BuildFirewallRulesetScript(const std::string& iface) -> std::string;

// The firmware-owned policy-routing adapter. Forks `ip`/`nft` (root) via the
// bounded argv-vector subprocess helper (never a shell). Hardware-bound — verified
// on-device; in the cross-compile container the `ip`/`nft` calls simply fail
// (bounded, no throw). Resolves the wired uplink interface itself from
// /sys/class/net.
class PolicyRouting final : public sst::network::IPolicyRouting {
   public:
    PolicyRouting() = default;
    ~PolicyRouting() override = default;

    PolicyRouting(const PolicyRouting&) = delete;
    auto operator=(const PolicyRouting&) -> PolicyRouting& = delete;
    PolicyRouting(PolicyRouting&&) = delete;
    auto operator=(PolicyRouting&&) -> PolicyRouting& = delete;

    auto ApplyUplinkRouting(const sst::network::UplinkRoutingConfig& cfg) -> bool override;
    auto TeardownUplinkRouting() -> void override;

    // F2 (B1): install ONLY the egress-only firewall on the detected wired interface
    // — NO routing table/rule — at process start, BEFORE boot-apply addresses
    // Ethernet. Guarantees no camera service (download/RTSP/control) is reachable on a
    // wired uplink from boot until the arbiter classifies Ethernet (a same-subnet LAN
    // commit removes it via TeardownUplinkRouting; a different-subnet uplink re-applies
    // the full routing + firewall via ApplyUplinkRouting). Uses the same atomic
    // `nft -f` transaction (F7). Idempotent; resolves the wired iface itself; bounded /
    // never throws (a no wired iface / absent nft is a logged no-op in the container).
    // Stateless (forks `nft`), so static — callable via an instance or the class.
    static auto ApplyEgressFirewall() -> void;
};

}  // namespace sst::adapters::control
