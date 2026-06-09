#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::config {

struct DeviceStaticIpData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> ip_address{std::nullopt};
    std::optional<std::string> subnet_mask{std::nullopt};
    std::optional<std::string> gateway{std::nullopt};
};

struct DeviceConnectivityWifiDirectData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> ssid{std::nullopt};
    std::optional<std::string> passphrase{std::nullopt};
    std::optional<std::uint32_t> channel{std::nullopt};
    std::optional<std::string> ip_address{std::nullopt};
};

struct DeviceConnectivityEthernetData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<DeviceStaticIpData> static_ip{std::nullopt};
};

struct DeviceConnectivityBluetoothData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> name{std::nullopt};
    std::optional<std::string> password{std::nullopt};
};

struct DeviceConnectivityData {
    std::optional<DeviceConnectivityWifiDirectData> wifi_direct{std::nullopt};
    std::optional<DeviceConnectivityEthernetData> ethernet{std::nullopt};
    std::optional<DeviceConnectivityBluetoothData> bluetooth{std::nullopt};
};

struct DeviceData {
    std::optional<std::string> name{std::nullopt};
    std::optional<std::string> model{std::nullopt};
    std::optional<std::string> version{std::nullopt};
    std::optional<std::string> serial_number{std::nullopt};
    std::optional<std::string> manufacturer{std::nullopt};
    std::optional<std::string> timezone{std::nullopt};
    std::optional<std::string> timestamp{std::nullopt};
    std::optional<DeviceConnectivityData> connectivity{std::nullopt};
};

}  // namespace sst::config
