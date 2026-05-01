#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::config {

// WiFi-Direct (Group Owner) bring-up credentials. Loaded from
// /etc/sst/cam/config/wifi-direct.json on first boot and persisted into the
// config_wifi_direct DB table by DbSeeder. Same content ships in every image.
struct WifiDirectData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> ssid{std::nullopt};
    std::optional<std::string> passphrase{std::nullopt};
    std::optional<std::uint32_t> channel{std::nullopt};
    std::optional<std::string> ip_address{std::nullopt};
};

}  // namespace sst::config
