#include "adapters/control/network/policy-routing.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "adapters/control/network/ip-ethernet-command.hpp"
#include "adapters/control/network/net-iface.hpp"
#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {

constexpr int kIpv4Bits = 32;
constexpr int kOctetBits = 8;
constexpr int kOctetCount = 4;
constexpr int kOctetMax = 255;
constexpr int kDecimalBase = 10;
constexpr std::uint32_t kFullMask = 0xFFFFFFFFU;
constexpr std::size_t kMaxIfaceName = 15;  // IFNAMSIZ - 1

// Parse a strict dotted-quad into a packed uint32, or nullopt on malformation.
auto ParseIpv4(const std::string& text) -> std::optional<std::uint32_t> {
    std::uint32_t packed = 0;
    std::size_t pos = 0;
    for (int octet = 0; octet < kOctetCount; ++octet) {
        if (pos >= text.size() || std::isdigit(static_cast<unsigned char>(text[pos])) == 0) {
            return std::nullopt;
        }
        int value = 0;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
            value = (value * kDecimalBase) + (text[pos] - '0');
            ++pos;
            if (value > kOctetMax) {
                return std::nullopt;
            }
        }
        packed = (packed << kOctetBits) | static_cast<std::uint32_t>(value);
        if (octet < kOctetCount - 1) {
            if (pos >= text.size() || text[pos] != '.') {
                return std::nullopt;
            }
            ++pos;
        }
    }
    return pos == text.size() ? std::optional<std::uint32_t>{packed} : std::nullopt;
}

auto MaskForPrefix(int prefix) -> std::uint32_t {
    return prefix == 0 ? 0U : (kFullMask << (kIpv4Bits - prefix));
}

// The bare address of a CIDR ("192.168.9.30/24" -> "192.168.9.30"; a bare IP is
// returned unchanged).
auto AddressOnly(const std::string& cidr) -> std::string {
    const auto slash = cidr.find('/');
    return slash == std::string::npos ? cidr : cidr.substr(0, slash);
}

auto FormatIpv4(std::uint32_t packed) -> std::string {
    std::string out;
    for (int shift = kIpv4Bits - kOctetBits; shift >= 0; shift -= kOctetBits) {
        out += std::to_string((packed >> static_cast<unsigned>(shift)) & kOctetMax);
        if (shift != 0) {
            out += ".";
        }
    }
    return out;
}

// Run a bounded state-change argv; log-and-continue on failure (idempotent teardown
// / self-healing apply — a missing prior rule is not fatal). Empty argv (a builder
// rejected its input) is skipped, never exec'd.
auto RunStep(const std::vector<std::string>& argv, const char* what) -> bool {
    if (argv.empty()) {
        spdlog::warn("PolicyRouting: skipped {} (invalid/unsafe argument)", what);
        return false;
    }
    const bool succeeded = RunBounded(argv, kApplyTimeout);
    if (!succeeded) {
        spdlog::info("PolicyRouting: {} returned non-zero (may be benign on re-apply/teardown)",
                     what);
    }
    return succeeded;
}

// The single-transaction firewall script path (root, /run — tmpfs). Fixed, not
// interpolated, so no untrusted token reaches the filesystem path.
constexpr const char* kFirewallScriptPath = "/run/sst-uplink-firewall.nft";

// F7: write the whole egress-only firewall ruleset to a temp file and apply it with a
// SINGLE `nft -f`, so nft swaps the table contents in ONE atomic transaction — no
// rule-less interval where inbound is accepted on the uplink. The script content is
// validated (BuildFirewallRulesetScript returns empty on an unsafe iface), so an
// un-whitelisted iface is never written or applied.
// Write `script` to the fixed /run path and apply it as one `nft -f` transaction.
// Quiet on failure: the caller decides whether a failure is fatal or a cue to fall
// back to a weaker-but-supported ruleset.
auto WriteAndApplyScript(const std::string& script, const char* what) -> bool {
    {
        std::ofstream file(kFirewallScriptPath, std::ios::trunc);
        if (!file) {
            spdlog::warn("PolicyRouting: could not write firewall script {}", kFirewallScriptPath);
            return false;
        }
        file << script;
    }
    if (RunBounded({"nft", "-f", kFirewallScriptPath}, kApplyTimeout)) {
        return true;
    }
    spdlog::debug("PolicyRouting: {} nft -f returned non-zero", what);
    return false;
}

auto WriteAndApplyFirewallScript(const std::string& iface) -> bool {
    const std::string stateful = BuildFirewallRulesetScript(iface);
    if (stateful.empty()) {
        spdlog::warn("PolicyRouting: refusing firewall script for unsafe iface '{}'", iface);
        return false;
    }
    if (WriteAndApplyScript(stateful, "stateful firewall ruleset")) {
        spdlog::info("PolicyRouting: egress-only firewall active on {} (stateful, ct state)",
                     iface);
        return true;
    }
    // The stateful ruleset needs nftables' `ct` expression, i.e. the nft_ct kernel
    // module. The JetPack 7.2 kernel (6.8.12-*-tegra) ships nft_chain_nat/compat/
    // masq/nat but NOT nft_ct, so `ct state established,related` is rejected there
    // with ENOENT no matter that nf_conntrack itself is loaded. Fall back to the
    // stateless equivalent rather than leaving the uplink unfiltered: dropping
    // inbound TCP whose flags are SYN-only blocks every externally-initiated
    // connection (so no camera service is reachable on the uplink) while the
    // SYN+ACK replies to the camera's own outbound connections still pass.
    const std::string stateless = BuildStatelessFirewallRulesetScript(iface);
    if (!stateless.empty() && WriteAndApplyScript(stateless, "stateless firewall ruleset")) {
        spdlog::warn(
            "PolicyRouting: egress-only firewall active on {} in STATELESS mode — this kernel has "
            "no nft_ct, so inbound TCP SYN is dropped instead of matching ct state",
            iface);
        return true;
    }
    // Both failed: the uplink has NO inbound filter. This is the security bound for
    // running as root (B1) — never downgrade it to an info-level "may be benign".
    spdlog::error(
        "PolicyRouting: FAILED to install any egress-only firewall on {} — camera services may be "
        "reachable on the uplink; refusing to treat this as benign",
        iface);
    return false;
}

}  // namespace

auto IsSafeIfaceName(const std::string& name) -> bool {
    if (name.empty() || name.size() > kMaxIfaceName || name.front() == '-') {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char chr) {
        return std::isalnum(chr) != 0 || chr == '.' || chr == '_' || chr == '-';
    });
}

auto NetworkCidrOf(const std::string& host_cidr) -> std::string {
    if (!IsIpv4Cidr(host_cidr)) {
        return {};
    }
    const auto slash = host_cidr.find('/');
    const auto address = ParseIpv4(host_cidr.substr(0, slash));
    if (!address) {
        return {};
    }
    const int prefix = std::stoi(host_cidr.substr(slash + 1));
    const std::uint32_t network = *address & MaskForPrefix(prefix);
    return FormatIpv4(network) + "/" + std::to_string(prefix);
}

auto BuildUplinkRuleAddArgv(const std::string& uplink_address) -> std::vector<std::string> {
    if (!IsIpv4Literal(uplink_address)) {
        return {};
    }
    return {"ip",
            "rule",
            "add",
            "from",
            uplink_address,
            "table",
            kUplinkRouteTable,
            "priority",
            kUplinkRulePriority};
}

auto BuildUplinkRuleDelArgv() -> std::vector<std::string> {
    return {"ip", "rule", "del", "priority", kUplinkRulePriority};
}

auto BuildUplinkSubnetRouteArgv(const std::string& subnet_cidr,
                                const std::string& iface) -> std::vector<std::string> {
    if (!IsIpv4Cidr(subnet_cidr) || !IsSafeIfaceName(iface)) {
        return {};
    }
    return {"ip", "route", "replace", subnet_cidr, "dev", iface, "table", kUplinkRouteTable};
}

auto BuildUplinkDefaultRouteArgv(const std::string& gateway,
                                 const std::string& iface) -> std::vector<std::string> {
    if (!IsIpv4Literal(gateway) || !IsSafeIfaceName(iface)) {
        return {};
    }
    return {"ip",    "route", "replace", "default", "via",
            gateway, "dev",   iface,     "table",   kUplinkRouteTable};
}

auto BuildUplinkRouteFlushArgv() -> std::vector<std::string> {
    return {"ip", "route", "flush", "table", kUplinkRouteTable};
}

auto BuildStaSubnetRouteArgv(const std::string& subnet_cidr,
                             const std::string& iface) -> std::vector<std::string> {
    if (!IsIpv4Cidr(subnet_cidr) || !IsSafeIfaceName(iface)) {
        return {};
    }
    return {"ip", "route", "replace", subnet_cidr, "dev", iface, "metric", "600"};
}

auto BuildFirewallAddTableArgv() -> std::vector<std::string> {
    return {"nft", "add", "table", "inet", kUplinkNftTable};
}

auto BuildFirewallFlushTableArgv() -> std::vector<std::string> {
    return {"nft", "flush", "table", "inet", kUplinkNftTable};
}

auto BuildFirewallAddChainArgv() -> std::vector<std::string> {
    // Each brace/semicolon is its own argv token; nft space-joins argv[1..] before
    // parsing, so this reconstructs the intended chain spec without any shell.
    // `policy accept` + an iface-scoped match => non-uplink (LAN) traffic is never
    // affected by this table.
    return {"nft",    "add",    "chain", "inet",  kUplinkNftTable, "input", "{",
            "type",   "filter", "hook",  "input", "priority",      "0",     ";",
            "policy", "accept", ";",     "}"};
}

auto BuildFirewallAllowEstablishedArgv(const std::string& iface) -> std::vector<std::string> {
    if (!IsSafeIfaceName(iface)) {
        return {};
    }
    return {"nft",     "add", "rule", "inet",  kUplinkNftTable,       "input",
            "iifname", iface, "ct",   "state", "established,related", "accept"};
}

auto BuildFirewallDropInboundArgv(const std::string& iface) -> std::vector<std::string> {
    if (!IsSafeIfaceName(iface)) {
        return {};
    }
    return {"nft", "add", "rule", "inet", kUplinkNftTable, "input", "iifname", iface, "drop"};
}

auto BuildFirewallDeleteTableArgv() -> std::vector<std::string> {
    return {"nft", "delete", "table", "inet", kUplinkNftTable};
}

auto BuildFirewallRulesetScript(const std::string& iface) -> std::string {
    // Fail closed: never emit a script that would interpolate an un-whitelisted iface
    // into an `iifname "<iface>"` literal.
    if (!IsSafeIfaceName(iface)) {
        return {};
    }
    const std::string table = std::string("inet ") + kUplinkNftTable;
    // add + flush + chain + rules as ONE nft script => one atomic transaction (F7).
    // `policy accept` + iface-scoped matches => non-uplink (LAN) traffic is untouched;
    // established/related is accepted (the camera's OUTBOUND streaming return path) and
    // everything else inbound on <iface> is dropped (no camera service on the uplink).
    std::string script;
    script += "add table " + table + "\n";
    script += "flush table " + table + "\n";
    script +=
        "add chain " + table + " input { type filter hook input priority 0; policy accept; }\n";
    script += "add rule " + table + " input iifname \"" + iface +
              "\" ip saddr 10.10.1.0/24 tcp dport 22 accept\n";
    script += "add rule " + table + " input iifname \"" + iface +
              "\" ct state established,related accept\n";
    script += "add rule " + table + " input iifname \"" + iface + "\" drop\n";
    return script;
}

auto BuildStatelessFirewallRulesetScript(const std::string& iface) -> std::string {
    if (!IsSafeIfaceName(iface)) {
        return {};
    }
    const std::string table = std::string("inet ") + kUplinkNftTable;
    // Conntrack-free equivalent for kernels without nft_ct (JetPack 7.2 — see
    // WriteAndApplyFirewallScript). `tcp flags syn / fin,syn,rst,ack` masks off those
    // four bits and matches SYN-ONLY segments: the opening packet of an inbound
    // connection. Dropping those makes every camera service (RTSP, control, download
    // — all TCP) unreachable from the uplink, while the SYN+ACK that answers the
    // camera's own outbound connections has two bits set and is not matched, so
    // egress and its return path keep working. Same single atomic transaction (F7).
    std::string script;
    script += "add table " + table + "\n";
    script += "flush table " + table + "\n";
    script +=
        "add chain " + table + " input { type filter hook input priority 0; policy accept; }\n";
    script += "add rule " + table + " input iifname \"" + iface +
              "\" ip saddr 10.10.1.0/24 tcp dport 22 accept\n";
    script += "add rule " + table + " input iifname \"" + iface +
              "\" tcp flags syn / fin,syn,rst,ack drop\n";
    return script;
}

auto PolicyRouting::ApplyUplinkRouting(const sst::network::UplinkRoutingConfig& cfg) -> bool {
    const std::string iface = DetectWiredIface();
    if (!IsSafeIfaceName(iface)) {
        spdlog::warn("PolicyRouting: no safe wired uplink iface resolved ('{}') — not applying",
                     iface);
        return false;
    }
    const std::string host = AddressOnly(cfg.uplink_address_cidr);
    const std::string subnet = NetworkCidrOf(cfg.uplink_address_cidr);
    spdlog::info(
        "PolicyRouting: applying different-subnet uplink routing iface={} addr={} subnet={}"
        " gw={} table={}",
        iface, host, subnet, cfg.uplink_gateway, kUplinkRouteTable);

    // Dedicated table: connected route (so `via` resolves) + default via the uplink
    // gateway. `ip route replace` is inherently idempotent.
    bool applied = RunStep(BuildUplinkSubnetRouteArgv(subnet, iface), "uplink connected route");
    if (!cfg.uplink_gateway.empty()) {
        applied = RunStep(BuildUplinkDefaultRouteArgv(cfg.uplink_gateway, iface),
                          "uplink default route") &&
                  applied;
    }
    // Rule: flush-then-add so a re-apply never stacks duplicate rules.
    RunStep(BuildUplinkRuleDelArgv(), "uplink rule (pre-clear)");
    applied = RunStep(BuildUplinkRuleAddArgv(host), "uplink source rule") && applied;

    // Egress-only firewall as a SINGLE atomic nft transaction (F7): a re-apply
    // (idempotent self-heal / carrier flap) swaps the whole ruleset at once, with NO
    // rule-less window where inbound is accepted on the uplink. THIS is the security
    // bound — no camera service is reachable on the uplink iface.
    applied = WriteAndApplyFirewallScript(iface) && applied;
    return applied;
}

auto PolicyRouting::ApplyEgressFirewall() -> void {
    const std::string iface = DetectWiredIface();
    if (!IsSafeIfaceName(iface)) {
        spdlog::info(
            "PolicyRouting: no safe wired iface for the boot egress firewall ('{}') — skipping "
            "(no wired uplink to protect yet)",
            iface);
        return;
    }
    // B1/F2: close the boot exposure window — a root listener (e.g. HttpDownloadServer
    // on 0.0.0.0:8080) must not be reachable on a wired uplink before the arbiter has
    // classified Ethernet. The same atomic transaction (F7) as the uplink firewall,
    // firewall-only (no routing table/rule), so a same-subnet-LAN commit later removes
    // it via TeardownUplinkRouting and a different-subnet uplink re-applies it.
    spdlog::info("PolicyRouting: installing boot egress-only firewall on {} (B1/F2)", iface);
    WriteAndApplyFirewallScript(iface);
}

auto PolicyRouting::TeardownUplinkRouting() -> void {
    spdlog::info("PolicyRouting: tearing down different-subnet uplink routing (table={})",
                 kUplinkRouteTable);
    RunStep(BuildUplinkRuleDelArgv(), "uplink rule del");
    RunStep(BuildUplinkRouteFlushArgv(), "uplink route flush");
    RunStep(BuildFirewallDeleteTableArgv(), "firewall delete table");
}

}  // namespace sst::adapters::control
