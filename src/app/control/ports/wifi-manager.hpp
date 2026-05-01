#pragma once

#include <cstdint>
#include <string>

namespace sst::control {

enum class WifiMode : std::uint8_t {
    kOff = 0,
    kP2pGroupOwner = 1,  // Jetson is WiFi-Direct GO (the only supported role)
};

struct WifiCredentials {
    std::string ssid;
    std::string passphrase;
};

struct WifiState {
    WifiMode mode{WifiMode::kOff};
    bool connected{false};
    std::string ssid;
    std::string ip_address;
};

// Abstraction over wpa_supplicant. The device only ever runs as a WiFi-Direct
// Group Owner — the companion app joins us, never the other way around — so
// the port surface is just bring-up + tear-down with the seeded credentials.
class IWifiManager {
   public:
    virtual ~IWifiManager() = default;

    virtual auto StartP2pGroupOwner(const WifiCredentials& creds) -> bool = 0;
    virtual auto Stop() -> void = 0;

    [[nodiscard]] virtual auto State() const -> WifiState = 0;
};

}  // namespace sst::control
