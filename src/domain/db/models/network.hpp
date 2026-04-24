#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::db {

enum class NetworkMedium : uint8_t { kWifi = 0, kEthernet = 1 };

namespace detail {
struct NetworkClientTag {};
struct NetworkAccessPointTag {};
}  // namespace detail

// NetworkClient and NetworkAccessPoint share identical fields.
// Template tag keeps them distinct types while allowing designated initializers.
template <typename Tag>
struct NetworkWifiNode {
    int64_t user_id{0};
    bool enabled{false};
    NetworkMedium medium{NetworkMedium::kWifi};
    std::optional<std::string> ssid;
    std::optional<std::string> wifi_password;
    bool static_ip{false};
    std::optional<std::string> ip_address;
    std::optional<std::string> subnet_mask;
    std::optional<std::string> gateway;
};

using NetworkClient = NetworkWifiNode<detail::NetworkClientTag>;
using NetworkAccessPoint = NetworkWifiNode<detail::NetworkAccessPointTag>;

struct NetworkBluetooth {
    int64_t user_id{0};
    bool enabled{true};
    std::string name;
    std::string password;
};

}  // namespace sst::db
