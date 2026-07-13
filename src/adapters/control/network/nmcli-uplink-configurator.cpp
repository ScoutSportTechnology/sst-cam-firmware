#include "adapters/control/network/nmcli-uplink-configurator.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "adapters/control/network/nmcli-command.hpp"
#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {

// The deterministic NM profile name for the app-provisioned STA uplink. A fixed
// name lets us re-provision (delete + recreate), bring up, and probe it without
// guessing an NM-generated connection id.
constexpr const char* kWifiStaProfile = "sst-uplink-wifi";
// Bounded wait for the wpa->NM ownership handoff to settle after `device set
// managed yes` (KTD2) — a just-freed P2P iface takes a moment to appear managed.
constexpr int kManagedPollAttempts = 6;
constexpr auto kManagedPollDelay = std::chrono::milliseconds{500};
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

auto NmcliUplinkConfigurator::WifiDeviceManaged(const std::string& device) -> bool {
    if (device.empty()) {
        return false;
    }
    // "DEVICE:STATE", e.g. "wlP1p1s0:connected" / "wlP1p1s0:unmanaged".
    const std::string out = Capture({"nmcli", "-t", "-f", "DEVICE,STATE", "device", "status"});
    std::istringstream stream(out);
    std::string line;
    while (std::getline(stream, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        if (line.substr(0, colon) == device) {
            return line.substr(colon + 1) != "unmanaged";
        }
    }
    return false;
}

auto NmcliUplinkConfigurator::ApplyWifiSta(const sst::config::WifiStaUplink& cfg)
    -> sst::network::UplinkResult {
    // The single-radio GO+STA concurrency hazard is designed out upstream: the
    // connectivity arbiter (KTD3) guarantees the WiFi-Direct GO is torn down
    // before this runs, so the radio is free. Our job is the wpa->NM ownership
    // handoff (KTD2) plus the explicit, app-provisioned STA join.
    if (cfg.ssid.value_or("").empty()) {
        return {.ok = false, .detail = "wifi uplink requires an SSID"};
    }

    const std::string device = DetectWifiDevice();
    if (device.empty()) {
        return {.ok = false, .detail = "no wifi device found"};
    }

    // Hand the just-freed radio back to NetworkManager, then wait (bounded) for it
    // to actually report managed — a hung/unmanaged iface returns a clear error
    // rather than silently stalling ApplyWifiSta.
    RunNmcli(BuildWifiManagedArgv(device));
    bool managed = false;
    for (int attempt = 0; attempt < kManagedPollAttempts; ++attempt) {
        if (WifiDeviceManaged(device)) {
            managed = true;
            break;
        }
        std::this_thread::sleep_for(kManagedPollDelay);
    }
    if (!managed) {
        return {.ok = false, .detail = "wifi interface not managed by NetworkManager"};
    }

    // Recreate the uplink profile from scratch so re-provisioning a different SSID
    // never inherits stale settings. Delete is best-effort (no prior profile is
    // fine).
    RunNmcli(BuildWifiStaDeleteArgv(kWifiStaProfile));
    const auto add_argv = BuildWifiStaAddArgv(kWifiStaProfile, device, cfg);
    if (add_argv.empty()) {
        return {.ok = false, .detail = "invalid wifi config (need SSID; static needs a CIDR)"};
    }
    if (!RunNmcli(add_argv)) {
        return {.ok = false, .detail = "nmcli con add (wifi) failed"};
    }
    if (!RunNmcli(BuildConnectionUpArgv(kWifiStaProfile))) {
        // The most common cause is a wrong passphrase / failed association. Drop
        // the dead profile so it can't linger, and report a clear reason (U7
        // renders it). The handler does not persist on !ok.
        RunNmcli(BuildWifiStaDeleteArgv(kWifiStaProfile));
        return {.ok = false, .detail = "failed to join wifi (check the password)"};
    }

    const std::string address = CurrentAddress(kWifiStaProfile);
    spdlog::info("NmcliUplinkConfigurator: wifi STA '{}' joined on {} ({})", cfg.ssid.value_or(""),
                 device, address);
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
    // Live state of the app-provisioned STA uplink: up == our profile is active
    // with an address. Absent profile (never provisioned, or reverted to the GO)
    // → not connected. Reality, independent of the last apply.
    const std::string address = CurrentAddress(kWifiStaProfile);
    if (address.empty()) {
        return {.ok = false, .detail = "not connected"};
    }
    return {.ok = true, .detail = address};
}

}  // namespace sst::adapters::control
