#include "adapters/control/network/ip-ethernet-configurator.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "adapters/control/network/ip-ethernet-command.hpp"
#include "adapters/control/network/net-iface.hpp"
#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {

// DHCP lease acquisition bound (`dhclient -1` makes a single lease attempt). Kept
// short enough that a no-lease link (carrier up, no server) feeds R21 "DHCP-
// pending / not-ready" quickly instead of hanging the dispatcher.
constexpr std::chrono::seconds kDhcpTimeout{20};

// The default ISC-dhclient leases database (root, default paths — U2 removed the
// non-root /tmp redirection). A lease block referencing our iface here is the
// ground-truth "this address came from DHCP" signal that cross-checks last_mode_.
constexpr const char* kDhcpLeasesFile = "/var/lib/dhcp/dhclient.leases";
// Where the firmware publishes DNS for a static config (the dhclient-script does
// this for the DHCP path).
constexpr const char* kResolvConf = "/etc/resolv.conf";

// Read a whole small sysfs/proc file, trimming trailing whitespace. Empty on any
// error (missing iface, admin-down carrier file, permission).
auto ReadTrimmed(const std::string& path) -> std::string {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' ||
                                content.back() == ' ' || content.back() == '\t')) {
        content.pop_back();
    }
    return content;
}

// Physical carrier state — /sys/class/net/<iface>/carrier is "1" only when the
// link is up (the file read fails / is "0" when the cable is out or admin-down).
auto CarrierUp(const std::string& iface) -> bool {
    return ReadTrimmed("/sys/class/net/" + iface + "/carrier") == "1";
}

// The IPv4 address (CIDR) currently bound on `iface`, empty if none. Parses
// `ip -o -4 addr show dev <iface>` → "... inet 10.10.1.30/24 ...".
auto IfaceIpv4Cidr(const std::string& iface) -> std::string {
    if (iface.empty()) {
        return {};
    }
    const std::string out =
        CaptureBounded({"ip", "-o", "-4", "addr", "show", "dev", iface}, kQueryTimeout).output;
    const auto inet = out.find("inet ");
    if (inet == std::string::npos) {
        return {};
    }
    const auto start = inet + 5;
    const auto end = out.find_first_of(" \t", start);
    return out.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// The default gateway reachable via `iface`, empty if none. Parses
// `ip -o -4 route show default dev <iface>` → "default via 10.10.1.1 ...".
auto DefaultGatewayOn(const std::string& iface) -> std::string {
    if (iface.empty()) {
        return {};
    }
    const std::string out =
        CaptureBounded({"ip", "-o", "-4", "route", "show", "default", "dev", iface}, kQueryTimeout)
            .output;
    const auto via = out.find("via ");
    if (via == std::string::npos) {
        return {};
    }
    const auto start = via + 4;
    const auto end = out.find_first_of(" \t", start);
    return out.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// Best-effort ground-truth: does the dhclient leases db hold a lease for `iface`?
// Cross-checks the firmware's own last_mode_ so a DHCP-leased address reports DHCP
// even on a fresh adapter instance (e.g. after a restart with the lease still
// held). Authoritative source stays last_mode_; this only rescues the unknown case.
auto DhcpLeaseActive(const std::string& iface) -> bool {
    std::ifstream file(kDhcpLeasesFile);
    if (!file) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content.find("interface \"" + iface + "\"") != std::string::npos;
}

// Write `body` to /etc/resolv.conf (truncating). `body` is already validated (a
// single nameserver line or empty). Empty body leaves the file untouched (the
// DHCP path's dhclient-script owns resolv.conf then).
auto WriteResolvConf(const std::string& body) -> void {
    if (body.empty()) {
        return;
    }
    std::ofstream file(kResolvConf, std::ios::trunc);
    if (!file) {
        spdlog::warn("IpEthernetConfigurator: could not write {}", kResolvConf);
        return;
    }
    file << body;
}

}  // namespace

auto IpEthernetConfigurator::ApplyStatic(const std::string& iface,
                                         const sst::config::EthernetUplink& cfg)
    -> sst::network::EthernetApplyResult {
    const std::string cidr = cfg.address.value_or("");
    const std::string gateway = cfg.gateway.value_or("");
    const std::string dns = cfg.dns.value_or("");

    // Content/injection safety: every BLE-supplied token is validated as a strict
    // IPv4 literal BEFORE it reaches an `ip` argv or the resolv.conf writer.
    if (!IsIpv4Cidr(cidr)) {
        return {.ok = false, .detail = "invalid static address (need IPv4 CIDR a.b.c.d/p)"};
    }
    if (!gateway.empty() && !IsIpv4Literal(gateway)) {
        return {.ok = false, .detail = "invalid gateway (need IPv4 literal)"};
    }
    if (!dns.empty() && !IsIpv4Literal(dns)) {
        return {.ok = false, .detail = "invalid DNS (need IPv4 literal)"};
    }

    const auto addr_argv = BuildEthAddrReplaceArgv(cidr, iface);
    if (addr_argv.empty() || !RunBounded(addr_argv, kApplyTimeout)) {
        return {.ok = false, .detail = "ip addr replace failed"};
    }
    if (!gateway.empty()) {
        const auto route_argv = BuildEthRouteReplaceArgv(gateway, iface);
        if (route_argv.empty() || !RunBounded(route_argv, kApplyTimeout)) {
            spdlog::warn("IpEthernetConfigurator: default route via {} on {} failed", gateway,
                         iface);
        }
    }
    WriteResolvConf(BuildResolvConf(dns));

    last_mode_ = sst::network::EthernetMode::kStatic;
    // Provenance: this address is ours, so teardown may remove it later.
    applied_address_ = cidr;
    const std::string address = IfaceIpv4Cidr(iface);
    spdlog::info("IpEthernetConfigurator: static ethernet up on {} ({})", iface,
                 address.empty() ? cidr : address);
    return {.ok = true, .detail = address.empty() ? cidr : address};
}

auto IpEthernetConfigurator::ApplyDhcp(const std::string& iface)
    -> sst::network::EthernetApplyResult {
    // Self-heal from a dirty prior state (e.g. a leftover static address WE set)
    // before leasing — otherwise the iface can hold both and readback is ambiguous.
    // Provenance-scoped: only our own previous address is removed. An out-of-band
    // address is left alone (removing it could strand the camera) — dhclient simply
    // adds the lease alongside it, which is untidy but never destructive.
    if (!applied_address_.empty()) {
        const auto del_argv = BuildEthAddrDelArgv(applied_address_, iface);
        if (!del_argv.empty()) {
            RunBounded(del_argv, kApplyTimeout);
        }
        applied_address_.clear();
    } else if (!IfaceIpv4Cidr(iface).empty()) {
        spdlog::warn(
            "IpEthernetConfigurator: {} already carries an address this firmware did not set — "
            "leasing alongside it rather than flushing it",
            iface);
    }
    // Root: dhclient uses its default lease/pid paths and its dhclient-script
    // writes /etc/resolv.conf directly (the non-root /tmp workaround is gone, U2).
    RunBounded({"dhclient", "-1", "-4", iface}, kDhcpTimeout);
    const std::string address = IfaceIpv4Cidr(iface);
    if (address.empty()) {
        // No lease within the bound: not-ready (feeds R21 DHCP-pending). Do NOT
        // throw / hang — the arbiter decides whether to wait or fall to STA.
        spdlog::warn("IpEthernetConfigurator: DHCP got no lease on {} within {}s", iface,
                     kDhcpTimeout.count());
        return {.ok = false, .detail = "no DHCP lease"};
    }
    last_mode_ = sst::network::EthernetMode::kDhcp;
    // Provenance: the lease is ours, so teardown may release exactly it.
    applied_address_ = address;
    spdlog::info("IpEthernetConfigurator: DHCP ethernet up on {} ({})", iface, address);
    return {.ok = true, .detail = address};
}

auto IpEthernetConfigurator::ApplyEthernet(const sst::config::EthernetUplink& cfg)
    -> sst::network::EthernetApplyResult {
    const std::string iface = DetectWiredIface();
    if (iface.empty()) {
        return {.ok = false, .detail = "no wired interface found"};
    }
    const bool dhcp = cfg.dhcp.value_or(true);
    return dhcp ? ApplyDhcp(iface) : ApplyStatic(iface, cfg);
}

auto IpEthernetConfigurator::ProbeEthernet() -> sst::network::EthernetProbe {
    const std::string iface = DetectWiredIface();
    if (iface.empty()) {
        return {};  // kDown, no address
    }
    const bool carrier = CarrierUp(iface);
    if (!carrier) {
        // Carrier down: report down with no address (tolerated, no throw) — even
        // if a stale address lingers on the iface, the link is not usable.
        return {.mode = sst::network::EthernetMode::kDown,
                .address = {},
                .carrier = false,
                .gateway = {}};
    }
    const std::string address = IfaceIpv4Cidr(iface);
    const auto mode =
        DeriveEthernetMode(last_mode_, carrier, !address.empty(), DhcpLeaseActive(iface));
    return {
        .mode = mode, .address = address, .carrier = carrier, .gateway = DefaultGatewayOn(iface)};
}

auto IpEthernetConfigurator::TeardownEthernet() -> void {
    const std::string iface = DetectWiredIface();
    if (iface.empty()) {
        return;
    }
    // Provenance gate (see applied_address_): if this firmware never applied an
    // address, whatever is on the NIC belongs to someone else — very likely the
    // operator's static management IP. Removing it would strand the camera with no
    // way back, so leave it and only forget our mode.
    if (applied_address_.empty()) {
        last_mode_ = sst::network::EthernetMode::kDown;
        spdlog::info(
            "IpEthernetConfigurator: ethernet teardown on {} left the existing address in place "
            "(not applied by this firmware — refusing to strand the camera)",
            iface);
        return;
    }
    // Remove exactly OUR address (and with it, its connected route + any default
    // route through it). Surgical rather than `ip addr flush`, so a co-present
    // out-of-band address survives. Idempotent: deleting an already-absent address
    // is a harmless non-zero.
    const auto del_argv = BuildEthAddrDelArgv(applied_address_, iface);
    if (!del_argv.empty()) {
        RunBounded(del_argv, kApplyTimeout);
    }
    spdlog::info("IpEthernetConfigurator: ethernet torn down on {} (removed {})", iface,
                 applied_address_);
    applied_address_.clear();
    last_mode_ = sst::network::EthernetMode::kDown;
}

auto IpEthernetConfigurator::ProbeDhcpOffer() -> std::optional<std::string> {
    const std::string iface = DetectWiredIface();
    if (iface.empty()) {
        return std::nullopt;
    }
    // Acquire the offer WITHOUT binding: a no-op `-sf /bin/true` client script
    // means dhclient records the offered lease to a scratch leases file but never
    // runs `ip addr add`, so the address is never applied to the interface. We
    // then parse the offer out and stop the (bounded) daemon. Scratch paths are
    // per-iface so concurrent probes on different ifaces don't collide.
    const std::string lease_file = "/run/sst-dhcp-offer-" + iface + ".leases";
    const std::string pid_file = "/run/sst-dhcp-offer-" + iface + ".pid";
    RunBounded(
        {"dhclient", "-1", "-4", "-sf", "/bin/true", "-lf", lease_file, "-pf", pid_file, iface},
        kDhcpTimeout);
    std::optional<std::string> offer;
    {
        std::ifstream file(lease_file);
        if (file) {
            const std::string content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
            offer = ParseDhcpOfferCidr(content);
        }
    }
    // Stop the probe daemon (best-effort) and clean up the scratch files so a
    // stale offer can't leak into the next probe.
    RunBounded({"dhclient", "-x", "-pf", pid_file}, kQueryTimeout);
    std::error_code rm_ec;
    std::filesystem::remove(lease_file, rm_ec);
    std::filesystem::remove(pid_file, rm_ec);
    return offer;
}

}  // namespace sst::adapters::control
