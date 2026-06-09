#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/network/models/wifi-direct-group.hpp"

namespace sst::control {

enum class WifiMode : std::uint8_t {
    kOff = 0,
    kP2pGroupOwner = 1,  // Jetson is WiFi-Direct GO (the only supported role)
};

struct WifiState {
    WifiMode mode{WifiMode::kOff};
    bool connected{false};
    std::string ssid;
    std::string ip_address;
};

// Abstraction over wpa_supplicant. The device only ever runs as an autonomous
// WiFi-Direct Group Owner — the companion app joins us, never the other way
// around. The GO generates its OWN SSID/passphrase (KTD4), so bring-up takes no
// input credentials and returns the actually-formed group for the camera to
// report over BLE.
class IWifiManager {
   public:
    virtual ~IWifiManager() = default;

    // Form an autonomous (non-persistent) P2P group owner and return its
    // generated SSID/passphrase + group interface + static GO IP. nullopt on
    // failure (e.g. no P2P-GO support, ctrl_iface unreachable).
    virtual auto StartP2pGroupOwner() -> std::optional<sst::network::WifiDirectGroup> = 0;

    virtual auto Stop() -> void = 0;

    [[nodiscard]] virtual auto State() const -> WifiState = 0;
};

}  // namespace sst::control
