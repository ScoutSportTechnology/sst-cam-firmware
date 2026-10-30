#include "adapters/control/network/policy-routing.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
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

// The bare address of a CIDR ("198.51.100.30/24" -> "198.51.100.30"; a bare IP is
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
constexpr const char* kXtChainA = "SST_UPLINK_A";
constexpr const char* kXtChainB = "SST_UPLINK_B";
std::optional<bool> nft_ct_supported;
std::string owned_main_gateway;

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

auto WiredIpv4Cidr(const std::string& iface) -> std::string {
    const auto result =
        CaptureBounded({"ip", "-o", "-4", "addr", "show", "dev", iface}, kQueryTimeout);
    if (!result.ok) {
        return {};
    }
    const auto inet = result.output.find("inet ");
    if (inet == std::string::npos) {
        return {};
    }
    const auto start = inet + 5;
    const auto end = result.output.find_first_of(" \t\n", start);
    const std::string cidr = result.output.substr(start, end - start);
    return IsIpv4Cidr(cidr) ? cidr : std::string{};
}

auto Xt(const char* tool, const std::string& operation, const std::string& chain) -> bool {
    return RunBounded({tool, "-w", operation, "INPUT", "-j", chain}, kApplyTimeout);
}

auto RemoveXtFirewall() -> void {
    for (const char* tool : {"iptables-nft", "ip6tables-nft"}) {
        for (const char* chain : {kXtChainA, kXtChainB}) {
            while (Xt(tool, "-C", chain) &&
                   RunStep({tool, "-w", "-D", "INPUT", "-j", chain}, "xt firewall detach")) {
            }
            RunStep({tool, "-w", "-F", chain}, "xt firewall flush");
            RunStep({tool, "-w", "-X", chain}, "xt firewall delete");
        }
    }
}

auto PopulateXtChain(const char* tool, const char* target, const std::string& iface,
                     const std::string& address) -> bool {
    if (!RunBounded({tool, "-w", "-N", target}, kApplyTimeout)) {
        if (RunBounded({tool, "-w", "-S", target}, kApplyTimeout)) {
            RunStep({tool, "-w", "-X", target}, "stale xt firewall delete");
        }
        if (!RunBounded({tool, "-w", "-N", target}, kApplyTimeout)) {
            return false;
        }
    }
    for (const char* rule : {"established", "dhcp", "ssh", "drop"}) {
        if (std::string(tool) == "ip6tables-nft" && std::string(rule) == "ssh" &&
            !address.empty()) {
            continue;
        }
        auto argv = BuildXtFirewallRuleArgv(iface, address, "-A", rule, target);
        argv.front() = tool;
        if (!RunBounded(argv, kApplyTimeout)) {
            return false;
        }
    }
    return true;
}

auto XtAttached(const std::string& rules, const char* chain) -> bool {
    return ("\n" + rules).find(std::string("\n-A INPUT -j ") + chain + "\n") != std::string::npos;
}

auto DetachOldXtChain(const char* tool, const char* old) -> bool {
    while (Xt(tool, "-C", old) &&
           RunStep({tool, "-w", "-D", "INPUT", "-j", old}, "old xt firewall detach")) {
    }
    if (Xt(tool, "-C", old)) {
        spdlog::error("PolicyRouting: old xt firewall jump still attached; refusing to flush");
        return false;
    }
    return RunStep({tool, "-w", "-X", old}, "old xt firewall delete");
}

auto InstallXtFirewall(const std::string& iface, const std::string& address) -> bool {
    const auto ipv4 = CaptureBounded({"iptables-nft", "-w", "-S", "INPUT"}, kQueryTimeout);
    const auto ipv6 = CaptureBounded({"ip6tables-nft", "-w", "-S", "INPUT"}, kQueryTimeout);
    if (!ipv4.ok || !ipv6.ok) {
        return false;
    }
    const bool chain_a_ipv4 = XtAttached(ipv4.output, kXtChainA);
    const bool chain_a_ipv6 = XtAttached(ipv6.output, kXtChainA);
    const bool chain_b_ipv4 = XtAttached(ipv4.output, kXtChainB);
    const bool chain_b_ipv6 = XtAttached(ipv6.output, kXtChainB);
    const bool use_chain_a = chain_a_ipv4 || chain_a_ipv6;
    const bool use_chain_b = chain_b_ipv4 || chain_b_ipv6;
    if (use_chain_a && use_chain_b) {
        return false;
    }
    if (chain_a_ipv4 != chain_a_ipv6 || chain_b_ipv4 != chain_b_ipv6) {
        spdlog::error("PolicyRouting: mismatched IPv4/IPv6 xt firewall jumps");
        return false;
    }
    const char* target = use_chain_a ? kXtChainB : kXtChainA;
    const char* old = use_chain_a ? kXtChainA : kXtChainB;
    if (!PopulateXtChain("iptables-nft", target, iface, address) ||
        !PopulateXtChain("ip6tables-nft", target, iface, address)) {
        return false;
    }
    if (!RunBounded({"ip6tables-nft", "-w", "-I", "INPUT", "1", "-j", target}, kApplyTimeout)) {
        return false;
    }
    if (!RunBounded({"iptables-nft", "-w", "-I", "INPUT", "1", "-j", target}, kApplyTimeout)) {
        RunStep({"ip6tables-nft", "-w", "-D", "INPUT", "-j", target},
                "unfinished xt firewall detach");
        return false;
    }
    return DetachOldXtChain("iptables-nft", old) && DetachOldXtChain("ip6tables-nft", old);
}

auto WriteAndApplyFirewallScript(const std::string& iface) -> bool {
    const std::string address = WiredIpv4Cidr(iface);
    const std::string stateful = BuildFirewallRulesetScript(iface, address);
    if (stateful.empty()) {
        spdlog::warn("PolicyRouting: refusing firewall script for unsafe iface '{}'", iface);
        return false;
    }
    if (!nft_ct_supported.has_value()) {
        nft_ct_supported = WriteAndApplyScript(stateful, "stateful firewall ruleset");
    } else if (*nft_ct_supported && !WriteAndApplyScript(stateful, "stateful firewall ruleset")) {
        nft_ct_supported = false;
    }
    if (*nft_ct_supported) {
        RemoveXtFirewall();
        spdlog::info("PolicyRouting: egress-only firewall active on {} (stateful, ct state)",
                     iface);
        return true;
    }
    const bool xt_ready = InstallXtFirewall(iface, address);
    if (SelectFirewallBackend(*nft_ct_supported, xt_ready) == FirewallBackend::kXt) {
        if (!RunBounded(BuildFirewallDeleteTableArgv(), kApplyTimeout) &&
            RunBounded({"nft", "list", "table", "inet", kUplinkNftTable}, kQueryTimeout)) {
            spdlog::error("PolicyRouting: obsolete nft firewall still active on {}", iface);
            return false;
        }
        spdlog::info("PolicyRouting: egress-only firewall active on {} (xt_conntrack)", iface);
        return true;
    }
    // If conntrack is unavailable, deny all unsolicited input on the wired
    // interface except LAN SSH. This blocks uplink return traffic too, so streaming
    // over this interface stays unavailable until the kernel supports stateful rules.
    const std::string stateless = BuildStatelessFirewallRulesetScript(iface, address);
    if (!stateless.empty() && WriteAndApplyScript(stateless, "stateless firewall ruleset")) {
        spdlog::error(
            "PolicyRouting: conntrack unavailable on {} — denying inbound traffic except LAN "
            "SSH; wired uplink return traffic and streaming are unavailable",
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

auto OwnedMainDefaultGateway(const std::string& routes) -> std::string {
    std::istringstream lines(routes);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.starts_with("default via ") || line.find("proto 186 ") == std::string::npos ||
            line.find("metric 90") == std::string::npos) {
            continue;
        }
        const auto start = std::string("default via ").size();
        const std::string gateway = line.substr(start, line.find(' ', start) - start);
        if (IsIpv4Literal(gateway)) {
            return gateway;
        }
    }
    return {};
}

auto IsOwnedMainDefaultRoute(const std::string& routes, const std::string& gateway) -> bool {
    return IsIpv4Literal(gateway) && OwnedMainDefaultGateway(routes) == gateway;
}

auto BuildUplinkMainDefaultRouteArgv(const std::string& gateway, const std::string& iface,
                                     const std::string& operation) -> std::vector<std::string> {
    if (!IsIpv4Literal(gateway) || !IsSafeIfaceName(iface) ||
        (operation != "replace" && operation != "del")) {
        return {};
    }
    return {"ip",  "route", operation, "default", "via",    gateway,
            "dev", iface,   "proto",   "186",     "metric", "90"};
}

auto BuildUplinkRouteFlushArgv() -> std::vector<std::string> {
    return {"ip", "route", "flush", "table", kUplinkRouteTable};
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

auto SelectFirewallBackend(bool nft_ct, bool xt_conntrack) -> FirewallBackend {
    if (nft_ct) {
        return FirewallBackend::kNft;
    }
    return xt_conntrack ? FirewallBackend::kXt : FirewallBackend::kStateless;
}

auto BuildXtFirewallRuleArgv(const std::string& iface, const std::string& address_cidr,
                             const std::string& operation, const std::string& rule,
                             const std::string& chain) -> std::vector<std::string> {
    if (!IsSafeIfaceName(iface) || (!address_cidr.empty() && !IsIpv4Cidr(address_cidr)) ||
        (operation != "-A" && operation != "-C") ||
        (chain != "SST_UPLINK" && chain != "SST_UPLINK_A" && chain != "SST_UPLINK_B")) {
        return {};
    }
    std::vector<std::string> argv{"iptables-nft", "-w", operation, chain, "-i", iface};
    if (rule == "established") {
        argv.insert(argv.end(),
                    {"-m", "conntrack", "--ctstate", "ESTABLISHED,RELATED", "-j", "ACCEPT"});
    } else if (rule == "dhcp") {
        argv.insert(argv.end(), {"-p", "udp", "--sport", "67", "--dport", "68", "-j", "ACCEPT"});
    } else if (rule == "ssh") {
        const auto subnet = NetworkCidrOf(address_cidr);
        if (!subnet.empty()) {
            argv.insert(argv.end(), {"-s", subnet});
        }
        argv.insert(argv.end(), {"-p", "tcp", "--dport", "22", "-j", "ACCEPT"});
    } else if (rule == "drop") {
        argv.insert(argv.end(), {"-j", "DROP"});
    } else {
        return {};
    }
    return argv;
}

auto BuildFirewallRulesetScript(const std::string& iface,
                                const std::string& address_cidr) -> std::string {
    if (!IsSafeIfaceName(iface) || (!address_cidr.empty() && !IsIpv4Cidr(address_cidr))) {
        return {};
    }
    const std::string subnet = NetworkCidrOf(address_cidr);
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
    script += "add rule " + table + " input iifname \"" + iface + "\" " +
              (subnet.empty() ? "" : "ip saddr " + subnet + " ") + "tcp dport 22 accept\n";
    script += "add rule " + table + " input iifname \"" + iface +
              "\" ct state established,related accept\n";
    script +=
        "add rule " + table + " input iifname \"" + iface + "\" udp sport 67 udp dport 68 accept\n";
    script += "add rule " + table + " input iifname \"" + iface + "\" drop\n";
    return script;
}

auto BuildStatelessFirewallRulesetScript(const std::string& iface,
                                         const std::string& address_cidr) -> std::string {
    if (!IsSafeIfaceName(iface) || (!address_cidr.empty() && !IsIpv4Cidr(address_cidr))) {
        return {};
    }
    const std::string subnet = NetworkCidrOf(address_cidr);
    const std::string table = std::string("inet ") + kUplinkNftTable;
    // Without conntrack, deny wired input except LAN SSH; do not present a
    // SYN-only filter as protection for UDP, IPv6 or non-SYN TCP packets.
    std::string script;
    script += "add table " + table + "\n";
    script += "flush table " + table + "\n";
    script +=
        "add chain " + table + " input { type filter hook input priority 0; policy accept; }\n";
    script += "add rule " + table + " input iifname \"" + iface + "\" " +
              (subnet.empty() ? "" : "ip saddr " + subnet + " ") + "tcp dport 22 accept\n";
    script +=
        "add rule " + table + " input iifname \"" + iface + "\" udp sport 67 udp dport 68 accept\n";
    script += "add rule " + table + " input iifname \"" + iface + "\" drop\n";
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
    if (cfg.uplink_gateway.empty()) {
        RunStep({"ip", "route", "del", "default", "table", kUplinkRouteTable},
                "uplink default route del");
    } else {
        applied = RunStep(BuildUplinkDefaultRouteArgv(cfg.uplink_gateway, iface),
                          "uplink default route") &&
                  applied;
    }
    // Rule: flush-then-add so a re-apply never stacks duplicate rules.
    RunStep(BuildUplinkRuleDelArgv(), "uplink rule (pre-clear)");
    applied = RunStep(BuildUplinkRuleAddArgv(host), "uplink source rule") && applied;

    const auto installed =
        CaptureBounded({"ip", "-o", "-4", "route", "show", "default", "dev", iface, "metric", "90"},
                       kQueryTimeout);
    if (installed.ok) {
        owned_main_gateway = OwnedMainDefaultGateway(installed.output);
        bool slot_available = installed.output.empty();
        if (!owned_main_gateway.empty() && owned_main_gateway != cfg.uplink_gateway &&
            RunStep(BuildUplinkMainDefaultRouteArgv(owned_main_gateway, iface, "del"),
                    "old uplink main default route")) {
            owned_main_gateway.clear();
            slot_available = true;
        }
        if (IsIpv4Literal(cfg.uplink_gateway) &&
            (slot_available || owned_main_gateway == cfg.uplink_gateway) &&
            RunStep(BuildUplinkMainDefaultRouteArgv(cfg.uplink_gateway, iface, "replace"),
                    "uplink main default route")) {
            owned_main_gateway = cfg.uplink_gateway;
        }
    }

    // Egress-only firewall as a SINGLE atomic nft transaction (F7): a re-apply
    // (idempotent self-heal / carrier flap) swaps the whole ruleset at once, with NO
    // rule-less window where inbound is accepted on the uplink. THIS is the security
    // bound — no camera service is reachable on the uplink iface.
    applied = WriteAndApplyFirewallScript(iface) && applied;
    return applied;
}

auto PolicyRouting::RefreshEgressFirewall() -> void { ApplyEgressFirewall(); }

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
    const std::string iface = DetectWiredIface();
    const auto installed = IsSafeIfaceName(iface)
                               ? CaptureBounded({"ip", "-o", "-4", "route", "show", "default",
                                                 "dev", iface, "metric", "90"},
                                                kQueryTimeout)
                               : CaptureResult{};
    if (installed.ok) {
        owned_main_gateway = OwnedMainDefaultGateway(installed.output);
    }
    if (!owned_main_gateway.empty() && installed.ok &&
        RunStep(BuildUplinkMainDefaultRouteArgv(owned_main_gateway, iface, "del"),
                "uplink main default route del")) {
        owned_main_gateway.clear();
    }
    RunStep(BuildFirewallDeleteTableArgv(), "firewall delete table");
    RemoveXtFirewall();
}

}  // namespace sst::adapters::control
