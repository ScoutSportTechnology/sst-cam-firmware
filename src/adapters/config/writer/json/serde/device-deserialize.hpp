#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/device.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, DeviceStaticIpData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("ip_address")) {
        values.ip_address = jsonObject.at("ip_address").get<std::string>();
    }
    if (jsonObject.contains("subnet_mask")) {
        values.subnet_mask = jsonObject.at("subnet_mask").get<std::string>();
    }
    if (jsonObject.contains("gateway")) {
        values.gateway = jsonObject.at("gateway").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiClientData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("wifi_ssid")) {
        values.wifi_ssid = jsonObject.at("wifi_ssid").get<std::string>();
    }
    if (jsonObject.contains("wifi_password")) {
        values.wifi_password = jsonObject.at("wifi_password").get<std::string>();
    }
    if (jsonObject.contains("static_ip")) {
        values.static_ip = jsonObject.at("static_ip").get<DeviceStaticIpData>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiAccessPointData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("ssid")) {
        values.ssid = jsonObject.at("ssid").get<std::string>();
    }
    if (jsonObject.contains("password")) {
        values.password = jsonObject.at("password").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiData& values) {
    if (jsonObject.contains("client")) {
        values.client = jsonObject.at("client").get<DeviceConnectivityWifiClientData>();
    }
    if (jsonObject.contains("access_point")) {
        values.access_point =
            jsonObject.at("access_point").get<DeviceConnectivityWifiAccessPointData>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityEthernetData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("static_ip")) {
        values.static_ip = jsonObject.at("static_ip").get<DeviceStaticIpData>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityBluetoothData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("name")) {
        values.name = jsonObject.at("name").get<std::string>();
    }
    if (jsonObject.contains("password")) {
        values.password = jsonObject.at("password").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, DeviceConnectivityData& values) {
    if (jsonObject.contains("wifi")) {
        values.wifi = jsonObject.at("wifi").get<DeviceConnectivityWifiData>();
    }
    if (jsonObject.contains("ethernet")) {
        values.ethernet = jsonObject.at("ethernet").get<DeviceConnectivityEthernetData>();
    }
    if (jsonObject.contains("bluetooth")) {
        values.bluetooth = jsonObject.at("bluetooth").get<DeviceConnectivityBluetoothData>();
    }
}

inline void from_json(const json& jsonObject, DeviceData& values) {
    if (jsonObject.contains("name")) {
        values.name = jsonObject.at("name").get<std::string>();
    }
    if (jsonObject.contains("model")) {
        values.model = jsonObject.at("model").get<std::string>();
    }
    if (jsonObject.contains("version")) {
        values.version = jsonObject.at("version").get<std::string>();
    }
    if (jsonObject.contains("serial_number")) {
        values.serial_number = jsonObject.at("serial_number").get<std::string>();
    }
    if (jsonObject.contains("manufacturer")) {
        values.manufacturer = jsonObject.at("manufacturer").get<std::string>();
    }
    if (jsonObject.contains("timezone")) {
        values.timezone = jsonObject.at("timezone").get<std::string>();
    }
    if (jsonObject.contains("timestamp")) {
        values.timestamp = jsonObject.at("timestamp").get<std::string>();
    }
    if (jsonObject.contains("connectivity")) {
        values.connectivity = jsonObject.at("connectivity").get<DeviceConnectivityData>();
    }
}

}  // namespace sst::config::domain
