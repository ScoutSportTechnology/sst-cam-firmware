#include "adapters/control/network/nmcli-command.hpp"

#include <string>
#include <vector>

namespace sst::adapters::control {

auto BuildWifiManagedArgv(const std::string& device) -> std::vector<std::string> {
    if (device.empty()) {
        return {};
    }
    return {"nmcli", "device", "set", device, "managed", "yes"};
}

auto BuildWifiStaDeleteArgv(const std::string& profile) -> std::vector<std::string> {
    if (profile.empty()) {
        return {};
    }
    return {"nmcli", "con", "delete", profile};
}

auto BuildWifiStaAddArgv(const std::string& profile, const std::string& device,
                         const sst::config::WifiStaUplink& cfg) -> std::vector<std::string> {
    const std::string ssid = cfg.ssid.value_or("");
    if (profile.empty() || device.empty() || ssid.empty()) {
        return {};
    }
    std::vector<std::string> argv{"nmcli", "con",    "add",  "type", "wifi", "con-name",
                                  profile, "ifname", device, "ssid", ssid};

    // WPA-PSK when a passphrase is present; an empty passphrase = an open network.
    const std::string psk = cfg.passphrase.value_or("");
    if (!psk.empty()) {
        argv.insert(argv.end(), {"wifi-sec.key-mgmt", "wpa-psk", "wifi-sec.psk", psk});
    }

    if (cfg.dhcp.value_or(true)) {
        argv.insert(argv.end(), {"ipv4.method", "auto"});
    } else {
        const std::string address = cfg.address.value_or("");
        if (address.find('/') == std::string::npos) {
            return {};  // a static uplink needs a CIDR address
        }
        argv.insert(argv.end(), {"ipv4.method", "manual", "ipv4.addresses", address});
        if (cfg.gateway.has_value() && !cfg.gateway->empty()) {
            argv.insert(argv.end(), {"ipv4.gateway", *cfg.gateway});
        }
        if (cfg.dns.has_value() && !cfg.dns->empty()) {
            argv.insert(argv.end(), {"ipv4.dns", *cfg.dns});
        }
    }

    // Never auto-activate (R-BOOT/INV1): only an app-confirmed migration brings
    // this profile up.
    argv.insert(argv.end(), {"connection.autoconnect", "no"});
    return argv;
}

}  // namespace sst::adapters::control
