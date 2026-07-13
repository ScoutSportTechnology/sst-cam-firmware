#include "adapters/control/network/nmcli-uplink-configurator.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "adapters/control/network/nmcli-command.hpp"
#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {

// The Jetson dedicates the WiFi radio (wlP1p1s0) to a firmware-owned
// wpa_supplicant for the WiFi-Direct GO, and NetworkManager is configured to
// NEVER manage it (conf.d unmanaged-devices). So the STA join is driven through
// that SAME wpa_supplicant via wpa_cli on its control socket, + dhclient for the
// address — NOT NetworkManager. Ctrl dir matches the daemon's `-O` path.
constexpr const char* kWpaCtrlDir = "/run/wpa_supplicant";
// Bounded wait for the STA association to reach wpa_state=COMPLETED.
constexpr int kAssocPollAttempts = 12;
constexpr auto kAssocPollDelay = std::chrono::milliseconds{500};
// DHCP lease acquisition bound (dhclient -1 exits after one lease attempt).
constexpr auto kDhcpTimeout = std::chrono::seconds{20};

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

// Run a wpa_cli command against the dedicated wpa_supplicant on `iface` and return
// its stdout. Bounded by the apply deadline.
auto WpaCli(const std::string& iface, const std::vector<std::string>& args) -> std::string {
    std::vector<std::string> argv{"wpa_cli", "-p", kWpaCtrlDir, "-i", iface};
    argv.insert(argv.end(), args.begin(), args.end());
    return CaptureBounded(argv, kApplyTimeout).output;
}
// Run a state-changing `nmcli` invocation, bounded by the apply deadline (a
// hung `con up` mid-NM-restart must not stall the BLE dispatcher thread).
auto RunNmcli(const std::vector<std::string>& argv) -> bool {
    return RunBounded(argv, kApplyTimeout);
}

// Capture stdout of a read-only `nmcli` query, bounded by the short query
// deadline. Used only for discovery (the connection name + current address),
// never to mutate state. Returns empty on timeout/failure.
auto Capture(const std::vector<std::string>& argv) -> std::string {
    return CaptureBounded(argv, kQueryTimeout).output;
}
}  // namespace

auto NmcliUplinkConfigurator::DetectEthernetConnection() -> std::string {
    // Lines: "DEVICE:TYPE:STATE:CONNECTION", e.g. "enP8p1s0:ethernet:connected:Wired connection 1".
    const std::string out =
        Capture({"nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device", "status"});
    std::istringstream stream(out);
    std::string line;
    while (std::getline(stream, line)) {
        // Split into at most 4 fields on ':' (CONNECTION itself may contain spaces
        // but not ':'); the connection name is everything after the 3rd colon.
        const auto col1 = line.find(':');
        if (col1 == std::string::npos) {
            continue;
        }
        const auto col2 = line.find(':', col1 + 1);
        const auto col3 = (col2 == std::string::npos) ? col2 : line.find(':', col2 + 1);
        if (col2 == std::string::npos || col3 == std::string::npos) {
            continue;
        }
        const std::string type = line.substr(col1 + 1, col2 - col1 - 1);
        const std::string state = line.substr(col2 + 1, col3 - col2 - 1);
        const std::string connection = line.substr(col3 + 1);
        // First managed ethernet device with a real connection (skip unmanaged
        // usb0/usb1 gadget ifaces, which have no connection).
        if (type == "ethernet" && state != "unmanaged" && !connection.empty() &&
            connection != "--") {
            return connection;
        }
    }
    return {};
}

auto NmcliUplinkConfigurator::CurrentAddress(const std::string& connection) -> std::string {
    if (connection.empty()) {
        return {};
    }
    // "IP4.ADDRESS[1]:10.10.1.30/24" → take the value after the colon.
    const std::string out =
        Capture({"nmcli", "-t", "-g", "IP4.ADDRESS", "con", "show", connection});
    std::string address = out;
    const auto newline = address.find('\n');
    if (newline != std::string::npos) {
        address.erase(newline);
    }
    return address;
}

auto NmcliUplinkConfigurator::ApplyEthernet(const sst::config::EthernetUplink& cfg)
    -> sst::network::UplinkResult {
    const std::string connection = DetectEthernetConnection();
    if (connection.empty()) {
        return {.ok = false, .detail = "no managed ethernet connection found"};
    }

    const bool dhcp = cfg.dhcp.value_or(true);
    const auto mod_argv = dhcp ? BuildEthDhcpArgv(connection) : BuildEthStaticArgv(connection, cfg);
    if (mod_argv.empty()) {
        return {.ok = false, .detail = "invalid static config (need address CIDR)"};
    }
    if (!RunNmcli(mod_argv)) {
        return {.ok = false, .detail = "nmcli con mod failed"};
    }
    if (!RunNmcli(BuildConnectionUpArgv(connection))) {
        return {.ok = false, .detail = "nmcli con up failed"};
    }

    const std::string address = CurrentAddress(connection);
    spdlog::info("NmcliUplinkConfigurator: ethernet '{}' up ({}, {})", connection,
                 dhcp ? "dhcp" : "static", address);
    return {.ok = true, .detail = address};
}

auto NmcliUplinkConfigurator::DetectWifiDevice() -> std::string {
    // Lines: "DEVICE:TYPE", e.g. "wlP1p1s0:wifi" and "p2p-dev-wlP1p1s0:wifi-p2p".
    // We want the base station device (TYPE exactly "wifi"), never the P2P iface.
    const std::string out = Capture({"nmcli", "-t", "-f", "DEVICE,TYPE", "device", "status"});
    std::istringstream stream(out);
    std::string line;
    while (std::getline(stream, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string device = line.substr(0, colon);
        const std::string type = line.substr(colon + 1);
        if (type == "wifi" && !device.empty()) {
            return device;
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

    // Add a fresh network on the dedicated wpa_supplicant. We do NOT `remove_network
    // all` — that would also drop the stored persistent P2P group the GO reuses.
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

    // NM does not manage this iface, so dhclient is free to lease an address (+ the
    // DHCP default route, which HasInternetUplink keys on for the streaming gate).
    RunBounded({"dhclient", "-1", "-4", iface}, kDhcpTimeout);
    const std::string address = IfaceIpv4(iface);
    if (address.empty()) {
        WpaCli(iface, {"disconnect"});
        WpaCli(iface, {"remove_network", net_id});
        return {.ok = false, .detail = "joined wifi but got no DHCP address"};
    }
    spdlog::info("NmcliUplinkConfigurator: wifi STA '{}' joined on {} ({})", ssid, iface, address);
    return {.ok = true, .detail = address};
}

auto NmcliUplinkConfigurator::ProbeEthernet() -> sst::network::UplinkResult {
    // Live state, independent of the firmware uplink config: the camera's
    // ethernet is NM-managed, so it may be up with an address even when the
    // uplink config has it disabled. up == a managed connection with an address.
    const std::string connection = DetectEthernetConnection();
    if (connection.empty()) {
        return {.ok = false, .detail = "no ethernet connection"};
    }
    const std::string address = CurrentAddress(connection);
    return {.ok = !address.empty(), .detail = address.empty() ? "no address" : address};
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
