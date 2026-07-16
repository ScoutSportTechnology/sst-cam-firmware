#include "adapters/control/network/net-iface.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace sst::adapters::control {

namespace {

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

}  // namespace

auto DetectWiredIface() -> std::string {
    for (const auto& name : SortedNetNames()) {
        static constexpr std::array<const char*, 9> kSkipPrefixes = {
            "lo", "docker", "veth", "br-", "virbr", "wl", "p2p", "rndis", "usb"};
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
