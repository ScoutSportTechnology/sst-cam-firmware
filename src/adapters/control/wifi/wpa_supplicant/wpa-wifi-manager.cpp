#include "adapters/control/wifi/wpa_supplicant/wpa-wifi-manager.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sst::adapters::control {

namespace {

constexpr std::size_t kRecvBufSize = 4096;
constexpr auto kRecvTimeout = std::chrono::seconds{2};

auto StartsWith(const std::string& s, std::string_view prefix) -> bool {
    return s.size() >= prefix.size() &&
           std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}

}  // namespace

WpaWifiManager::WpaWifiManager(std::string iface, std::string ctrl_dir)
    : iface_(std::move(iface)), ctrl_dir_(std::move(ctrl_dir)) {}

WpaWifiManager::~WpaWifiManager() { CloseCtrlSocket(); }

auto WpaWifiManager::OpenCtrlSocket() -> bool {
    if (sock_ >= 0) {
        return true;
    }

    sock_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_ < 0) {
        spdlog::error("WpaWifiManager: socket() failed: {}", std::strerror(errno));
        return false;
    }

    sockaddr_un local{};
    local.sun_family = AF_UNIX;
    local_path_ = fmt::format("/tmp/wpa_ctrl_{}-{}", ::getpid(),
                              static_cast<int>(std::rand()));
    std::strncpy(local.sun_path, local_path_.c_str(), sizeof(local.sun_path) - 1);
    ::unlink(local.sun_path);
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        spdlog::error("WpaWifiManager: bind({}) failed: {}", local_path_,
                      std::strerror(errno));
        CloseCtrlSocket();
        return false;
    }

    sockaddr_un remote{};
    remote.sun_family = AF_UNIX;
    const auto remote_path = fmt::format("{}/{}", ctrl_dir_, iface_);
    std::strncpy(remote.sun_path, remote_path.c_str(), sizeof(remote.sun_path) - 1);
    if (::connect(sock_, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) < 0) {
        spdlog::error("WpaWifiManager: connect({}) failed: {}", remote_path,
                      std::strerror(errno));
        CloseCtrlSocket();
        return false;
    }

    timeval tv{};
    tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(kRecvTimeout).count();
    ::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    spdlog::info("WpaWifiManager: ctrl_iface connected via {}", remote_path);
    return true;
}

auto WpaWifiManager::CloseCtrlSocket() -> void {
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
    }
    if (!local_path_.empty()) {
        ::unlink(local_path_.c_str());
        local_path_.clear();
    }
}

auto WpaWifiManager::SendCommand(std::string_view cmd) -> std::optional<std::string> {
    if (sock_ < 0 && !OpenCtrlSocket()) {
        return std::nullopt;
    }

    if (::send(sock_, cmd.data(), cmd.size(), 0) < 0) {
        spdlog::error("WpaWifiManager: send(\"{}\") failed: {}", cmd, std::strerror(errno));
        return std::nullopt;
    }

    std::array<char, kRecvBufSize> buf{};
    const auto n = ::recv(sock_, buf.data(), buf.size() - 1, 0);
    if (n < 0) {
        spdlog::error("WpaWifiManager: recv after \"{}\" failed: {}", cmd,
                      std::strerror(errno));
        return std::nullopt;
    }
    return std::string(buf.data(), static_cast<std::size_t>(n));
}

auto WpaWifiManager::RemoveAllNetworks() -> void {
    SendCommand("DISCONNECT");
    SendCommand("REMOVE_NETWORK all");
}

auto WpaWifiManager::ConfigureNetwork(const sst::control::WifiCredentials& creds, int mode)
    -> std::optional<int> {
    auto add = SendCommand("ADD_NETWORK");
    if (!add) {
        return std::nullopt;
    }
    int net_id = 0;
    {
        const std::string& s = *add;
        auto end = s.find('\n');
        const auto sv = std::string_view(s).substr(0, end);
        if (std::from_chars(sv.data(), sv.data() + sv.size(), net_id).ec != std::errc{}) {
            spdlog::error("WpaWifiManager: ADD_NETWORK returned non-numeric: \"{}\"", sv);
            return std::nullopt;
        }
    }

    auto set = [this, net_id](std::string_view key, std::string_view value) -> bool {
        auto reply = SendCommand(fmt::format("SET_NETWORK {} {} {}", net_id, key, value));
        return reply && StartsWith(*reply, "OK");
    };

    if (!set("ssid", fmt::format("\"{}\"", creds.ssid))) return std::nullopt;
    if (!set("psk", fmt::format("\"{}\"", creds.passphrase))) return std::nullopt;
    if (!set("key_mgmt", "WPA-PSK")) return std::nullopt;
    if (!set("proto", "RSN")) return std::nullopt;
    if (!set("pairwise", "CCMP")) return std::nullopt;
    if (!set("group", "CCMP")) return std::nullopt;
    if (!set("mode", std::to_string(mode))) return std::nullopt;
    if (mode == 2) {
        // AP / GO on 2.4GHz channel 1 — broadest device support.
        if (!set("frequency", "2412")) return std::nullopt;
    }

    auto enable = SendCommand(fmt::format("ENABLE_NETWORK {}", net_id));
    if (!enable || !StartsWith(*enable, "OK")) {
        return std::nullopt;
    }
    auto select = SendCommand(fmt::format("SELECT_NETWORK {}", net_id));
    if (!select || !StartsWith(*select, "OK")) {
        return std::nullopt;
    }
    SendCommand("SAVE_CONFIG");
    return net_id;
}

auto WpaWifiManager::StartClient(const sst::control::WifiCredentials& creds) -> bool {
    std::lock_guard lock(mtx_);
    if (!OpenCtrlSocket()) return false;
    RemoveAllNetworks();
    auto net_id = ConfigureNetwork(creds, /*mode=*/0);
    if (!net_id) return false;
    state_ = {.mode = sst::control::WifiMode::kClient,
              .connected = true,
              .ssid = creds.ssid,
              .ip_address = ""};
    spdlog::info("WpaWifiManager::StartClient ssid=\"{}\" net_id={}", creds.ssid, *net_id);
    return true;
}

auto WpaWifiManager::StartAccessPoint(const sst::control::WifiCredentials& creds) -> bool {
    std::lock_guard lock(mtx_);
    if (!OpenCtrlSocket()) return false;
    RemoveAllNetworks();
    auto net_id = ConfigureNetwork(creds, /*mode=*/2);
    if (!net_id) return false;
    state_ = {.mode = sst::control::WifiMode::kAccessPoint,
              .connected = true,
              .ssid = creds.ssid,
              .ip_address = "192.168.49.1"};
    spdlog::info("WpaWifiManager::StartAccessPoint ssid=\"{}\" net_id={}", creds.ssid,
                 *net_id);
    return true;
}

auto WpaWifiManager::StartP2pGroupOwner(const sst::control::WifiCredentials& creds) -> bool {
    // From the companion app's perspective, a WPA2-PSK AP with a known
    // SSID/passphrase is functionally equivalent to a WiFi-Direct GO and is
    // far simpler/more reliable to bring up via wpa_supplicant ctrl_iface.
    std::lock_guard lock(mtx_);
    if (!OpenCtrlSocket()) return false;
    RemoveAllNetworks();
    auto net_id = ConfigureNetwork(creds, /*mode=*/2);
    if (!net_id) return false;
    state_ = {.mode = sst::control::WifiMode::kP2pGroupOwner,
              .connected = true,
              .ssid = creds.ssid,
              .ip_address = "192.168.49.1"};
    spdlog::info("WpaWifiManager::StartP2pGroupOwner ssid=\"{}\" net_id={}", creds.ssid,
                 *net_id);
    return true;
}

auto WpaWifiManager::Stop() -> void {
    std::lock_guard lock(mtx_);
    if (sock_ >= 0) {
        SendCommand("DISCONNECT");
        SendCommand("REMOVE_NETWORK all");
    }
    state_ = {};
    CloseCtrlSocket();
    spdlog::info("WpaWifiManager::Stop");
}

auto WpaWifiManager::State() const -> sst::control::WifiState {
    std::lock_guard lock(mtx_);
    return state_;
}

}  // namespace sst::adapters::control
