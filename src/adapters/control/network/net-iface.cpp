#include "adapters/control/network/net-iface.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace sst::adapters::control {

namespace {

// ARPHRD_ETHER as sysfs reports it in /sys/class/net/<if>/type. Anything else is
// not an Ethernet-class link: the Jetson also exposes can0 (ARPHRD_CAN, 280) and
// lo (ARPHRD_LOOPBACK, 772), neither of which can carry the camera's LAN/uplink.
constexpr const char* kArphrdEther = "1";

auto SortedNetNames() -> std::vector<std::string> {
    namespace fs = std::filesystem;
    std::error_code dir_ec;
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator("/sys/class/net", dir_ec)) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());  // stable pick across boots
    return names;
}

auto HasWirelessDir(const std::string& name) -> bool {
    std::error_code exists_ec;
    return std::filesystem::exists("/sys/class/net/" + name + "/wireless", exists_ec);
}

// Whole small sysfs file, trailing whitespace trimmed. Empty on any error.
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

}  // namespace

auto DetectWiredIface() -> std::string {
    for (const auto& name : SortedNetNames()) {
        // l4tbr0 is the Tegra USB-gadget bridge: ARPHRD_ETHER but virtual, and it
        // is not the camera's physical LAN or uplink.
        static constexpr std::array<const char*, 10> kSkipPrefixes = {
            "lo", "docker", "veth", "br-", "virbr", "wl", "p2p", "rndis", "usb", "l4tbr"};
        bool skip = (name == "lo");
        for (const auto* prefix : kSkipPrefixes) {
            if (name.starts_with(prefix)) {
                skip = true;
                break;
            }
        }
        if (skip || HasWirelessDir(name)) {
            continue;
        }
        // The type gate is load-bearing, not belt-and-braces: without it the
        // alphabetically-first netdev on a Jetson is can0 (ARPHRD_CAN), which the
        // firewall/probe callers would then target instead of the real Ethernet —
        // silently disabling the egress-only bound and Ethernet-WAN egress
        // selection. Mirrors deploy/install.sh detect_eth_iface.
        if (ReadTrimmed("/sys/class/net/" + name + "/type") != kArphrdEther) {
            continue;
        }
        return name;
    }
    return {};
}

auto DetectWifiStaIface() -> std::string {
    for (const auto& name : SortedNetNames()) {
        if (name.starts_with("p2p") || !HasWirelessDir(name)) {
            continue;
        }
        return name;
    }
    return {};
}

}  // namespace sst::adapters::control
