#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/device_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const DefaultDeviceStaticIpConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["ip_address"] = value.ip_address;
    jsonObject["subnet_mask"] = value.subnet_mask;
    jsonObject["gateway"] = value.gateway;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiClientConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["wifi_ssid"] = value.wifi_ssid;
    jsonObject["wifi_password"] = value.wifi_password;
    jsonObject["static_ip"] = value.static_ip;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiAccessPointConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["ssid"] = value.ssid;
    jsonObject["password"] = value.password;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiConfig& value) {
    jsonObject = json::object();
    jsonObject["client"] = value.client;
    jsonObject["access_point"] = value.access_point;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityEthernetConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["static_ip"] = value.static_ip;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityBluetoothConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["name"] = value.name;
    jsonObject["password"] = value.password;
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityConfig& value) {
    jsonObject = json::object();
    jsonObject["wifi"] = value.wifi;
    jsonObject["ethernet"] = value.ethernet;
    jsonObject["bluetooth"] = value.bluetooth;
}

inline void to_json(json& jsonObject, const DefaultDeviceConfig& value) {
    jsonObject = json::object();
    jsonObject["name"] = value.name;
    jsonObject["model"] = value.model;
    jsonObject["version"] = value.version;
    jsonObject["serial_number"] = value.serial_number;
    jsonObject["manufacturer"] = value.manufacturer;
    jsonObject["timezone"] = value.timezone;
    jsonObject["timestamp"] = value.timestamp;
    jsonObject["connectivity"] = value.connectivity;
}

inline void to_json(json& jsonObject, const UserDeviceStaticIpConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.ip_address.has_value()) {
        jsonObject["ip_address"] = value.ip_address.value();
    }
    if (value.subnet_mask.has_value()) {
        jsonObject["subnet_mask"] = value.subnet_mask.value();
    }
    if (value.gateway.has_value()) {
        jsonObject["gateway"] = value.gateway.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityWifiClientConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.wifi_ssid.has_value()) {
        jsonObject["wifi_ssid"] = value.wifi_ssid.value();
    }
    if (value.wifi_password.has_value()) {
        jsonObject["wifi_password"] = value.wifi_password.value();
    }
    if (value.static_ip.has_value()) {
        jsonObject["static_ip"] = value.static_ip.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityWifiAccessPointConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.ssid.has_value()) {
        jsonObject["ssid"] = value.ssid.value();
    }
    if (value.password.has_value()) {
        jsonObject["password"] = value.password.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityWifiConfig& value) {
    jsonObject = json::object();
    if (value.client.has_value()) {
        jsonObject["client"] = value.client.value();
    }
    if (value.access_point.has_value()) {
        jsonObject["access_point"] = value.access_point.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityEthernetConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.static_ip.has_value()) {
        jsonObject["static_ip"] = value.static_ip.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityBluetoothConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.name.has_value()) {
        jsonObject["name"] = value.name.value();
    }
    if (value.password.has_value()) {
        jsonObject["password"] = value.password.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConnectivityConfig& value) {
    jsonObject = json::object();
    if (value.wifi.has_value()) {
        jsonObject["wifi"] = value.wifi.value();
    }
    if (value.ethernet.has_value()) {
        jsonObject["ethernet"] = value.ethernet.value();
    }
    if (value.bluetooth.has_value()) {
        jsonObject["bluetooth"] = value.bluetooth.value();
    }
}

inline void to_json(json& jsonObject, const UserDeviceConfig& value) {
    jsonObject = json::object();
    if (value.name.has_value()) {
        jsonObject["name"] = value.name.value();
    }
    if (value.model.has_value()) {
        jsonObject["model"] = value.model.value();
    }
    if (value.version.has_value()) {
        jsonObject["version"] = value.version.value();
    }
    if (value.serial_number.has_value()) {
        jsonObject["serial_number"] = value.serial_number.value();
    }
    if (value.manufacturer.has_value()) {
        jsonObject["manufacturer"] = value.manufacturer.value();
    }
    if (value.timezone.has_value()) {
        jsonObject["timezone"] = value.timezone.value();
    }
    if (value.timestamp.has_value()) {
        jsonObject["timestamp"] = value.timestamp.value();
    }
    if (value.connectivity.has_value()) {
        jsonObject["connectivity"] = value.connectivity.value();
    }
}

}  // namespace sst::config::domain
