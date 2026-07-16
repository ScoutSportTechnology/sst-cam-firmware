#include "adapters/control/network/nmcli-uplink-configurator.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "adapters/control/network/gratuitous-arp.hpp"
#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {

// The Jetson dedicates the WiFi radio (wlP1p1s0) to a firmware-owned
// wpa_supplicant for the WiFi-Direct GO, and NetworkManager is configured to
// NEVER manage it (conf.d unmanaged-devices). So the STA join is driven through
// that SAME wpa_supplicant via wpa_cli on its control socket, + dhclient for the
// address — NOT NetworkManager. Ctrl dir matches the daemon's `-O` path.
constexpr const char* kWpaCtrlDir = "/run/wpa_supplicant";
// Bounded wait for the STA association to reach wpa_state=COMPLETED. A post-GO
// radio may need to scan + associate + 4-way handshake, so allow a generous
// window (a busy 2.4GHz AP can take >6s).
constexpr int kAssocPollAttempts = 30;
constexpr auto kAssocPollDelay = std::chrono::milliseconds{500};
// DHCP lease acquisition bound (dhclient -1 exits after one lease attempt).
constexpr auto kDhcpTimeout = std::chrono::seconds{20};
// Regulatory-domain scan bounds (AdoptApRegDomain): trigger the scan via wpa_cli
// then read the cached results. Kept short so a slow/again scan can never stall
// the BLE command past the app's response timeout.
constexpr auto kRegScanTimeout = std::chrono::seconds{5};
constexpr auto kRegScanSettle = std::chrono::seconds{3};

// The last whitespace-separated integer token in `text` (wpa_cli add_network
// prints the new network id on its own line). Empty if none.
auto LastNumber(const std::string& text) -> std::string {
    std::istringstream stream(text);
    std::string token;
    std::string last;
    while (stream >> token) {
        if (!token.empty() && std::all_of(token.begin(), token.end(), [](unsigned char chr) {
                return std::isdigit(chr) != 0;
            })) {
            last = token;
        }
    }
    return last;
}

// The IPv4 address (no prefix length) currently on `iface`, empty if none.
// Parses `ip -4 -o addr show dev <iface>` → "... inet 192.168.1.50/24 ...".
auto IfaceIpv4(const std::string& iface) -> std::string {
    if (iface.empty()) {
        return {};
    }
    const std::string out =
        CaptureBounded({"ip", "-4", "-o", "addr", "show", "dev", iface}, kQueryTimeout).output;
    const auto inet = out.find("inet ");
    if (inet == std::string::npos) {
        return {};
    }
    const auto start = inet + 5;
    const auto end = out.find_first_of("/ \t", start);
    return out.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// The radio boots at regulatory domain "00" (world), under which the operating
// channels are flagged no-initiate/passive: the driver can SEE an AP in a scan
// but is forbidden to START an association, so the join scans fine yet never
// reaches wpa_state=COMPLETED. Move off "00" by adopting the target network's
// own advertised 802.11d country before associating. The country is read from
// the air (the AP's Country IE) each join and never stored — the camera stays
// stateless about location. Best-effort: if no AP advertises 11d, leave the
// domain as-is and let the association proceed.
//
// The scan is TRIGGERED through the wpa_supplicant that owns the iface (a raw
// `iw dev … scan` conflicts with the running supplicant and can block for the
// full apply timeout — long enough to blow past the app's BLE-command timeout,
// which surfaces as a bare "setNetworkConfig failed"). The results are then read
// from the kernel's cache (`scan dump` — no radio op, no conflict, instant).
// Both steps are bounded short so this can never stall the command dispatcher.
auto AdoptApRegDomain(const std::string& iface) -> void {
    CaptureBounded({"wpa_cli", "-p", kWpaCtrlDir, "-i", iface, "scan"}, kRegScanTimeout);
    std::this_thread::sleep_for(kRegScanSettle);
    const std::string scan =
        CaptureBounded({"iw", "dev", iface, "scan", "dump"}, kRegScanTimeout).output;
    // Lines look like: "\tCountry: US\tEnvironment: Indoor/Outdoor".
    const auto pos = scan.find("Country: ");
    if (pos == std::string::npos) {
        return;
    }
    const std::string country = scan.substr(pos + 9, 2);
    const auto is_alpha = [](char chr) {
        return std::isalpha(static_cast<unsigned char>(chr)) != 0;
    };
    if (country.size() != 2 || country == "00" || country == "XX" || !is_alpha(country[0]) ||
        !is_alpha(country[1])) {
        return;
    }
    RunBounded({"iw", "reg", "set", country}, kQueryTimeout);
    spdlog::info("NmcliUplinkConfigurator: adopted regulatory domain {} from AP beacon (was 00)",
                 country);
}

// Run a wpa_cli command against the dedicated wpa_supplicant on `iface` and return
// its stdout. Bounded by the apply deadline.
auto WpaCli(const std::string& iface, const std::vector<std::string>& args) -> std::string {
    std::vector<std::string> argv{"wpa_cli", "-p", kWpaCtrlDir, "-i", iface};
    argv.insert(argv.end(), args.begin(), args.end());
    return CaptureBounded(argv, kApplyTimeout).output;
}

// The wpa network ids of the STATION networks we previously added — i.e. every
// LIST_NETWORKS row whose SSID is NOT a DIRECT-* P2P group. Lets us remove stale
// STA networks from earlier provisions (they accumulate on the long-lived daemon
// and can confuse a fresh join) WITHOUT dropping the persistent P2P group.
// LIST_NETWORKS rows are `id<TAB>ssid<TAB>bssid<TAB>flags` after a header line.
auto StaleStaNetworkIds(const std::string& iface) -> std::vector<std::string> {
    std::vector<std::string> ids;
    std::istringstream stream(WpaCli(iface, {"list_networks"}));
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (first) {  // header: "network id / ssid / bssid / flags"
            first = false;
            continue;
        }
        const auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) {
            continue;
        }
        const auto tab2 = line.find('\t', tab1 + 1);
        const std::string network_id = line.substr(0, tab1);
        const std::string ssid =
            line.substr(tab1 + 1, tab2 == std::string::npos ? std::string::npos : tab2 - tab1 - 1);
        if (!network_id.empty() && !ssid.starts_with("DIRECT-")) {
            ids.push_back(network_id);
        }
    }
    return ids;
}
// Capture stdout of a read-only query, bounded by the short query deadline. Used
// only for discovery (the WiFi device name), never to mutate state. Returns
// empty on timeout/failure.
auto Capture(const std::vector<std::string>& argv) -> std::string {
    return CaptureBounded(argv, kQueryTimeout).output;
}
}  // namespace

auto NmcliUplinkConfigurator::DetectWifiDevice() -> std::string {
    // Do NOT use nmcli here: wlP1p1s0 is NM-UNMANAGED (dedicated wpa_supplicant for
    // the WiFi-Direct GO), and nmcli's device list is unreliable for it — right
    // after a P2P group teardown it can report no wifi device at all. Read the
    // kernel netdev list directly and pick the base WiFi station iface: a name
    // starting "wl" (the "p2p-dev-*" virtual iface starts "p2p", so it is skipped).
    const std::string out = Capture({"ip", "-o", "link", "show"});
    std::istringstream stream(out);
    std::string line;
    while (std::getline(stream, line)) {
        // "3: wlP1p1s0: <BROADCAST,...> mtu ..." — the name follows the first ": ".
        const auto first = line.find(": ");
        if (first == std::string::npos) {
            continue;
        }
        const auto start = first + 2;
        const auto end = line.find(':', start);
        if (end == std::string::npos) {
            continue;
        }
        std::string name = line.substr(start, end - start);
        // Drop a "wlP1p1s0@if3"-style suffix if the kernel added one.
        const auto at_pos = name.find('@');
        if (at_pos != std::string::npos) {
            name = name.substr(0, at_pos);
        }
        if (name.starts_with("wl")) {
            return name;
        }
    }
    return {};
}

auto NmcliUplinkConfigurator::ApplyWifiSta(const sst::config::WifiStaUplink& cfg)
    -> sst::network::UplinkResult {
    // The Jetson dedicates wlP1p1s0 to a firmware-owned wpa_supplicant for the
    // WiFi-Direct GO; NetworkManager is configured to NEVER manage it. So the STA
    // join runs through that SAME wpa_supplicant (wpa_cli on its ctrl socket) +
    // dhclient — NOT NetworkManager. The connectivity arbiter has already torn the
    // GO down (INV2), so the radio is free to associate as a station.
    const std::string ssid = cfg.ssid.value_or("");
    if (ssid.empty()) {
        return {.ok = false, .detail = "wifi uplink requires an SSID"};
    }
    const std::string iface = DetectWifiDevice();
    if (iface.empty()) {
        return {.ok = false, .detail = "no wifi device found"};
    }

    // Adopt the target network's 802.11d country before associating — the radio
    // boots at domain "00", under which association is forbidden on the operating
    // channels (scan sees the AP, but the join never completes). Stateless: the
    // country is read from the AP's beacon each join, never persisted.
    AdoptApRegDomain(iface);

    // Remove stale STA networks from prior provisions first (they accumulate on
    // the long-lived daemon and can confuse a fresh join); keep the DIRECT-* P2P
    // group the GO reuses. Then add a fresh network.
    for (const auto& stale : StaleStaNetworkIds(iface)) {
        WpaCli(iface, {"remove_network", stale});
    }
    const std::string net_id = LastNumber(WpaCli(iface, {"add_network"}));
    if (net_id.empty()) {
        return {.ok = false, .detail = "wpa_cli add_network failed"};
    }
    WpaCli(iface, {"set_network", net_id, "ssid", "\"" + ssid + "\""});
    const std::string psk = cfg.passphrase.value_or("");
    if (psk.empty()) {
        WpaCli(iface, {"set_network", net_id, "key_mgmt", "NONE"});
    } else {
        WpaCli(iface, {"set_network", net_id, "psk", "\"" + psk + "\""});
    }
    WpaCli(iface, {"enable_network", net_id});
    WpaCli(iface, {"select_network", net_id});

    // Wait (bounded) for association.
    bool associated = false;
    for (int attempt = 0; attempt < kAssocPollAttempts; ++attempt) {
        if (WpaCli(iface, {"status"}).find("wpa_state=COMPLETED") != std::string::npos) {
            associated = true;
            break;
        }
        std::this_thread::sleep_for(kAssocPollDelay);
    }
    if (!associated) {
        // Most likely a wrong passphrase / SSID not found. Tear the attempt down so
        // it can't linger, and report a clear reason (U7 renders it). The handler
        // does not persist on !ok.
        WpaCli(iface, {"disconnect"});
        WpaCli(iface, {"remove_network", net_id});
        return {.ok = false, .detail = "failed to join wifi (check the password)"};
    }

    // Flush the stale WiFi-Direct GO address (192.168.49.1) the group left on the
    // iface FIRST — otherwise the iface holds both it and the STA lease, and
    // IfaceIpv4 returns the GO IP (which the app then probes and, finding it dead,
    // reverts). cap_net_admin lets the firmware do this without root.
    RunBounded({"ip", "addr", "flush", "dev", iface}, kApplyTimeout);
    // NM does not manage this iface, so dhclient is free to lease an address (+ the
    // DHCP default route, which HasInternetUplink keys on for the streaming gate).
    // The firmware runs as root (U2), so dhclient uses its default lease/pid paths
    // (/var/lib/dhcp, /var/run) and its dhclient-script writes /etc/resolv.conf
    // directly — the old /tmp lease/pid redirection (a non-root workaround) is gone.
    RunBounded({"dhclient", "-1", "-4", iface}, kDhcpTimeout);
    const std::string address = IfaceIpv4(iface);
    if (address.empty()) {
        WpaCli(iface, {"disconnect"});
        WpaCli(iface, {"remove_network", net_id});
        return {.ok = false, .detail = "joined wifi but got no DHCP address"};
    }
    spdlog::info("NmcliUplinkConfigurator: wifi STA '{}' joined on {} ({})", ssid, iface, address);
    // Best-effort: broadcast a gratuitous ARP for the just-assigned STA IP so a
    // phone caching a stale/negative entry for it (from between migrations) MAY
    // refresh sooner and connect on the first tap. Not load-bearing — the app's
    // wider probe retry (U2) is what actually closes the one-tap gap; a neighbour
    // is free to ignore an unsolicited ARP for a FAILED entry.
    SendGratuitousArp(iface, address);
    return {.ok = true, .detail = address};
}

auto NmcliUplinkConfigurator::TeardownWifiSta() -> void {
    const std::string iface = DetectWifiDevice();
    if (iface.empty()) {
        return;
    }
    // Release the lease + kill the dhclient we started, disconnect the wpa
    // association, then flush any address + routes off the radio so nothing (a
    // stale default/subnet route, a leftover STA IP) can linger to fight another
    // interface or dirty the iface for the GO. Root uses dhclient's default
    // lease/pid paths (U2), so no /tmp redirection is needed to match the client.
    RunBounded({"dhclient", "-r", "-4", iface}, kDhcpTimeout);
    WpaCli(iface, {"disconnect"});
    RunBounded({"ip", "addr", "flush", "dev", iface}, kApplyTimeout);
    RunBounded({"ip", "route", "flush", "dev", iface}, kApplyTimeout);
    spdlog::info("NmcliUplinkConfigurator: wifi STA torn down on {}", iface);
}

auto NmcliUplinkConfigurator::ProbeWifiSta() -> sst::network::UplinkResult {
    // Live STA state on the dedicated wpa_supplicant: associated to a real AP (not
    // the WiFi-Direct GO) with an address. `mode=P2P GO` / a DIRECT- SSID means the
    // GO is up, i.e. NOT a station uplink.
    const std::string iface = DetectWifiDevice();
    if (iface.empty()) {
        return {.ok = false, .detail = "no wifi device"};
    }
    const std::string status = WpaCli(iface, {"status"});
    const bool completed = status.find("wpa_state=COMPLETED") != std::string::npos;
    const bool is_go = status.find("mode=P2P GO") != std::string::npos ||
                       status.find("ssid=DIRECT-") != std::string::npos;
    const std::string address = completed && !is_go ? IfaceIpv4(iface) : std::string{};
    if (address.empty()) {
        return {.ok = false, .detail = "not connected"};
    }
    return {.ok = true, .detail = address};
}

}  // namespace sst::adapters::control
