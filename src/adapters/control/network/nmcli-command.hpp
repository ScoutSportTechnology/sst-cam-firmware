#pragma once

#include <string>
#include <vector>

#include "domain/config/models/uplink-config.hpp"

namespace sst::adapters::control {

// Pure builders for the `nmcli` argv that configure the camera's WiFi-STA uplink
// connection. Factored out of the fork/exec adapter so argv construction is
// unit-testable without touching NetworkManager (the exec needs a live NM + the
// real connection and is verified on-device). Each returns an empty vector on
// invalid input so the caller fails fast instead of exec'ing a half-formed
// command. (Ethernet argv builders moved out with the firmware-owned Ethernet
// controller, which drives `ip`/`dhclient` directly, not nmcli.)

// `nmcli device set <dev> managed yes` — the wpa_supplicant->NetworkManager
// ownership handoff (KTD2). After the WiFi-Direct GO is torn down the base wlan
// device is free but NM may still have it marked unmanaged (the firmware drove
// P2P on it); this hands it back to NM so a STA join can bind it.
auto BuildWifiManagedArgv(const std::string& device) -> std::vector<std::string>;

// `nmcli con delete <profile>` — drop a stale uplink-wifi profile before
// (re)creating it, so re-provisioning a different SSID doesn't collide.
auto BuildWifiStaDeleteArgv(const std::string& profile) -> std::vector<std::string>;

// `nmcli con add type wifi con-name <profile> ifname <dev> ssid <ssid>
//  [wifi-sec.key-mgmt wpa-psk wifi-sec.psk <psk>]
//  (ipv4.method auto | ipv4.method manual ipv4.addresses <cidr> [gw] [dns])
//  connection.autoconnect no` — the STA uplink profile. `autoconnect no` enforces
// R-BOOT/INV1: the camera never self-joins on boot; the profile is only ever
// brought up explicitly by an app-confirmed migration. Empty vector on invalid
// input (no SSID, or static without a CIDR address) so the caller fails fast.
auto BuildWifiStaAddArgv(const std::string& profile, const std::string& device,
                         const sst::config::WifiStaUplink& cfg) -> std::vector<std::string>;

}  // namespace sst::adapters::control
