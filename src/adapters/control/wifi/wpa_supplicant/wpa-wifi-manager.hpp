#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "app/control/ports/wifi-manager.hpp"

namespace sst::adapters::control {

// wpa_supplicant ctrl_iface adapter. Talks to the running wpa_supplicant
// daemon over a UNIX datagram socket at <ctrl_dir>/<iface>, sending plain-text
// commands like ADD_NETWORK / SET_NETWORK / SELECT_NETWORK / P2P_GROUP_ADD and
// reading back the OK/FAIL/<id> responses.
//
// Both AP mode and "WiFi-Direct GO" use the same network-block path here:
// mode=2 + WPA2-PSK with the supplied SSID/passphrase. From the phone's
// perspective this is indistinguishable from a regular WiFi AP, which is what
// the companion app needs (auto-join with hardcoded credentials).
class WpaWifiManager final : public sst::control::IWifiManager {
   public:
    explicit WpaWifiManager(std::string iface = "wlan0",
                            std::string ctrl_dir = "/run/wpa_supplicant");
    ~WpaWifiManager() override;

    WpaWifiManager(const WpaWifiManager&) = delete;
    auto operator=(const WpaWifiManager&) -> WpaWifiManager& = delete;
    WpaWifiManager(WpaWifiManager&&) = delete;
    auto operator=(WpaWifiManager&&) -> WpaWifiManager& = delete;

    auto StartClient(const sst::control::WifiCredentials& creds) -> bool override;
    auto StartAccessPoint(const sst::control::WifiCredentials& creds) -> bool override;
    auto StartP2pGroupOwner(const sst::control::WifiCredentials& creds) -> bool override;
    auto Stop() -> void override;
    [[nodiscard]] auto State() const -> sst::control::WifiState override;

   private:
    [[nodiscard]] auto OpenCtrlSocket() -> bool;
    auto CloseCtrlSocket() -> void;
    auto SendCommand(std::string_view cmd) -> std::optional<std::string>;
    auto ConfigureNetwork(const sst::control::WifiCredentials& creds, int mode)
        -> std::optional<int>;
    auto RemoveAllNetworks() -> void;

    std::string iface_;
    std::string ctrl_dir_;
    int sock_{-1};
    std::string local_path_;

    mutable std::mutex mtx_;
    sst::control::WifiState state_{};
};

}  // namespace sst::adapters::control
