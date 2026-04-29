#pragma once

#include <cstdint>
#include <string>

namespace sst::control {

enum class WifiMode : std::uint8_t {
    kOff = 0,
    kClient = 1,        // Jetson joins an existing AP
    kAccessPoint = 2,   // Jetson hosts an AP
    kP2pGroupOwner = 3, // Jetson is WiFi-Direct GO
    kP2pClient = 4,     // Jetson is WiFi-Direct client
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

// Abstraction over wpa_supplicant (or NetworkManager / nmcli). Lets controllers
// bring up an AP, join a network, or stand up a WiFi-Direct group on demand
// without depending on the underlying ctrl_iface protocol.
class IWifiManager {
   public:
    virtual ~IWifiManager() = default;

    virtual auto StartClient(const WifiCredentials& creds) -> bool = 0;
    virtual auto StartAccessPoint(const WifiCredentials& creds) -> bool = 0;
    virtual auto StartP2pGroupOwner(const WifiCredentials& creds) -> bool = 0;
    virtual auto Stop() -> void = 0;

    virtual auto State() const -> WifiState = 0;
};

}  // namespace sst::control
