#include "adapters/control/network/policy-routing.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
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

    // Egress-only firewall: create+flush the table (idempotent reset), re-add the
    // chain + rules. THIS is the security bound — no camera service is reachable on
    // the uplink iface.
    RunStep(BuildFirewallAddTableArgv(), "firewall table");
    RunStep(BuildFirewallFlushTableArgv(), "firewall flush");
    applied = RunStep(BuildFirewallAddChainArgv(), "firewall chain") && applied;
    applied =
        RunStep(BuildFirewallAllowEstablishedArgv(iface), "firewall allow established") && applied;
    applied = RunStep(BuildFirewallDropInboundArgv(iface), "firewall drop inbound") && applied;
    return applied;
}

auto PolicyRouting::TeardownUplinkRouting() -> void {
    spdlog::info("PolicyRouting: tearing down different-subnet uplink routing (table={})",
                 kUplinkRouteTable);
    RunStep(BuildUplinkRuleDelArgv(), "uplink rule del");
    RunStep(BuildUplinkRouteFlushArgv(), "uplink route flush");
    RunStep(BuildFirewallDeleteTableArgv(), "firewall delete table");
}

}  // namespace sst::adapters::control
