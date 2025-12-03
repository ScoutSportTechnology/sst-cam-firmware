#pragma once

#include <optional>
#include <string>

#include "domain/config/config_files.hpp"

namespace domain::config {

struct DefaultDeviceStaticIpConfig {
    bool enabled{false};
    std::string ip_address;
    std::string subnet_mask;
    std::string gateway;
};

struct DefaultDeviceConnectivityWifiClientConfig {
    bool enabled{false};
    std::string wifi_ssid;
    std::string wifi_password;
    DefaultDeviceStaticIpConfig static_ip;
};

struct DefaultDeviceConnectivityWifiAccessPointConfig {
    bool enabled{false};
    std::string ssid;
    std::string password;
};

struct DefaultDeviceConnectivityWifiConfig {
    DefaultDeviceConnectivityWifiClientConfig client;
    DefaultDeviceConnectivityWifiAccessPointConfig access_point;
};

struct DefaultDeviceConnectivityEthernetConfig {
    bool enabled{false};
    DefaultDeviceStaticIpConfig static_ip;
};

struct DefaultDeviceConnectivityBluetoothConfig {
    bool enabled{false};
    std::string name;
    std::string password;
};

struct DefaultDeviceConnectivityConfig {
    DefaultDeviceConnectivityWifiConfig wifi;
    DefaultDeviceConnectivityEthernetConfig ethernet;
    DefaultDeviceConnectivityBluetoothConfig bluetooth;
};

struct DefaultDeviceConfig {
    std::string name;
    std::string model;
    std::string version;
    std::string serial_number;
    std::string manufacturer;
    std::string timezone;
    std::string timestamp;
    DefaultDeviceConnectivityConfig connectivity;
};

struct UserDeviceStaticIpConfig {
    std::optional<bool> enabled;
    std::optional<std::string> ip_address;
    std::optional<std::string> subnet_mask;
    std::optional<std::string> gateway;
};

struct UserDeviceConnectivityWifiClientConfig {
    std::optional<bool> enabled;
    std::optional<std::string> wifi_ssid;
    std::optional<std::string> wifi_password;
    std::optional<UserDeviceStaticIpConfig> static_ip;
};

struct UserDeviceConnectivityWifiAccessPointConfig {
    std::optional<bool> enabled;
    std::optional<std::string> ssid;
    std::optional<std::string> password;
};

struct UserDeviceConnectivityWifiConfig {
    std::optional<UserDeviceConnectivityWifiClientConfig> client;
    std::optional<UserDeviceConnectivityWifiAccessPointConfig> access_point;
};

struct UserDeviceConnectivityEthernetConfig {
    std::optional<bool> enabled;
    std::optional<UserDeviceStaticIpConfig> static_ip;
};

struct UserDeviceConnectivityBluetoothConfig {
    std::optional<bool> enabled;
    std::optional<std::string> name;
    std::optional<std::string> password;
};

struct UserDeviceConnectivityConfig {
    std::optional<UserDeviceConnectivityWifiConfig> wifi;
    std::optional<UserDeviceConnectivityEthernetConfig> ethernet;
    std::optional<UserDeviceConnectivityBluetoothConfig> bluetooth;
};

struct UserDeviceConfig {
    std::optional<std::string> name;
    std::optional<std::string> model;
    std::optional<std::string> version;
    std::optional<std::string> serial_number;
    std::optional<std::string> manufacturer;
    std::optional<std::string> timezone;
    std::optional<std::string> timestamp;
    std::optional<UserDeviceConnectivityConfig> connectivity;
};

using DeviceConfig = ConfigFiles<DefaultDeviceConfig, UserDeviceConfig>;

}  // namespace domain::config
