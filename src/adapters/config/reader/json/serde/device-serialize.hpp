#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/device.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const DeviceStaticIpData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.ip_address) {
        jsonObject["ip_address"] = *values.ip_address;
    }
    if (values.subnet_mask) {
        jsonObject["subnet_mask"] = *values.subnet_mask;
    }
    if (values.gateway) {
        jsonObject["gateway"] = *values.gateway;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiClientData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.wifi_ssid) {
        jsonObject["wifi_ssid"] = *values.wifi_ssid;
    }
    if (values.wifi_password) {
        jsonObject["wifi_password"] = *values.wifi_password;
    }
    if (values.static_ip) {
        jsonObject["static_ip"] = *values.static_ip;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiAccessPointData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.ssid) {
        jsonObject["ssid"] = *values.ssid;
    }
    if (values.password) {
        jsonObject["password"] = *values.password;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiData& values) {
    jsonObject = json::object();

    if (values.client) {
        jsonObject["client"] = *values.client;
    }
    if (values.access_point) {
        jsonObject["access_point"] = *values.access_point;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityEthernetData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.static_ip) {
        jsonObject["static_ip"] = *values.static_ip;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityBluetoothData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.name) {
        jsonObject["name"] = *values.name;
    }
    if (values.password) {
        jsonObject["password"] = *values.password;
    }
}

inline void to_json(json& jsonObject, const DeviceConnectivityData& values) {
    jsonObject = json::object();

    if (values.wifi) {
        jsonObject["wifi"] = *values.wifi;
    }
    if (values.ethernet) {
        jsonObject["ethernet"] = *values.ethernet;
    }
    if (values.bluetooth) {
        jsonObject["bluetooth"] = *values.bluetooth;
    }
}

inline void to_json(json& jsonObject, const DeviceData& values) {
    jsonObject = json::object();

    if (values.name) {
        jsonObject["name"] = *values.name;
    }
    if (values.model) {
        jsonObject["model"] = *values.model;
    }
    if (values.version) {
        jsonObject["version"] = *values.version;
    }
    if (values.serial_number) {
        jsonObject["serial_number"] = *values.serial_number;
    }
    if (values.manufacturer) {
        jsonObject["manufacturer"] = *values.manufacturer;
    }
    if (values.timezone) {
        jsonObject["timezone"] = *values.timezone;
    }
    if (values.timestamp) {
        jsonObject["timestamp"] = *values.timestamp;
    }
    if (values.connectivity) {
        jsonObject["connectivity"] = *values.connectivity;
    }
}

}  // namespace sst::config::domain
