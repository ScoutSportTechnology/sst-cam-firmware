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

inline void from_json(const json& jsonObject, DefaultDeviceStaticIpConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("ip_address").get_to(value.ip_address);
    jsonObject.at("subnet_mask").get_to(value.subnet_mask);
    jsonObject.at("gateway").get_to(value.gateway);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiClientConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["wifi_ssid"] = value.wifi_ssid;
    jsonObject["wifi_password"] = value.wifi_password;
    jsonObject["static_ip"] = value.static_ip;
}

inline void from_json(const json& jsonObject, DefaultDeviceConnectivityWifiClientConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("wifi_ssid").get_to(value.wifi_ssid);
    jsonObject.at("wifi_password").get_to(value.wifi_password);
    jsonObject.at("static_ip").get_to(value.static_ip);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiAccessPointConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["ssid"] = value.ssid;
    jsonObject["password"] = value.password;
}

inline void from_json(const json& jsonObject,
                      DefaultDeviceConnectivityWifiAccessPointConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("ssid").get_to(value.ssid);
    jsonObject.at("password").get_to(value.password);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityWifiConfig& value) {
    jsonObject = json::object();
    jsonObject["client"] = value.client;
    jsonObject["access_point"] = value.access_point;
}

inline void from_json(const json& jsonObject, DefaultDeviceConnectivityWifiConfig& value) {
    jsonObject.at("client").get_to(value.client);
    jsonObject.at("access_point").get_to(value.access_point);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityEthernetConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["static_ip"] = value.static_ip;
}

inline void from_json(const json& jsonObject, DefaultDeviceConnectivityEthernetConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("static_ip").get_to(value.static_ip);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityBluetoothConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["name"] = value.name;
    jsonObject["password"] = value.password;
}

inline void from_json(const json& jsonObject, DefaultDeviceConnectivityBluetoothConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("name").get_to(value.name);
    jsonObject.at("password").get_to(value.password);
}

inline void to_json(json& jsonObject, const DefaultDeviceConnectivityConfig& value) {
    jsonObject = json::object();
    jsonObject["wifi"] = value.wifi;
    jsonObject["ethernet"] = value.ethernet;
    jsonObject["bluetooth"] = value.bluetooth;
}

inline void from_json(const json& jsonObject, DefaultDeviceConnectivityConfig& value) {
    jsonObject.at("wifi").get_to(value.wifi);
    jsonObject.at("ethernet").get_to(value.ethernet);
    jsonObject.at("bluetooth").get_to(value.bluetooth);
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

inline void from_json(const json& jsonObject, DefaultDeviceConfig& value) {
    jsonObject.at("name").get_to(value.name);
    jsonObject.at("model").get_to(value.model);
    jsonObject.at("version").get_to(value.version);
    jsonObject.at("serial_number").get_to(value.serial_number);
    jsonObject.at("manufacturer").get_to(value.manufacturer);
    jsonObject.at("timezone").get_to(value.timezone);
    jsonObject.at("timestamp").get_to(value.timestamp);
    jsonObject.at("connectivity").get_to(value.connectivity);
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

inline void from_json(const json& jsonObject, UserDeviceStaticIpConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("ip_address")) {
        value.ip_address = jsonObject.at("ip_address").get<std::string>();
    }
    if (jsonObject.contains("subnet_mask")) {
        value.subnet_mask = jsonObject.at("subnet_mask").get<std::string>();
    }
    if (jsonObject.contains("gateway")) {
        value.gateway = jsonObject.at("gateway").get<std::string>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityWifiClientConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("wifi_ssid")) {
        value.wifi_ssid = jsonObject.at("wifi_ssid").get<std::string>();
    }
    if (jsonObject.contains("wifi_password")) {
        value.wifi_password = jsonObject.at("wifi_password").get<std::string>();
    }
    if (jsonObject.contains("static_ip")) {
        value.static_ip = jsonObject.at("static_ip").get<UserDeviceStaticIpConfig>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityWifiAccessPointConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("ssid")) {
        value.ssid = jsonObject.at("ssid").get<std::string>();
    }
    if (jsonObject.contains("password")) {
        value.password = jsonObject.at("password").get<std::string>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityWifiConfig& value) {
    if (jsonObject.contains("client")) {
        value.client = jsonObject.at("client").get<UserDeviceConnectivityWifiClientConfig>();
    }
    if (jsonObject.contains("access_point")) {
        value.access_point =
            jsonObject.at("access_point").get<UserDeviceConnectivityWifiAccessPointConfig>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityEthernetConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("static_ip")) {
        value.static_ip = jsonObject.at("static_ip").get<UserDeviceStaticIpConfig>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityBluetoothConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("name")) {
        value.name = jsonObject.at("name").get<std::string>();
    }
    if (jsonObject.contains("password")) {
        value.password = jsonObject.at("password").get<std::string>();
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

inline void from_json(const json& jsonObject, UserDeviceConnectivityConfig& value) {
    if (jsonObject.contains("wifi")) {
        value.wifi = jsonObject.at("wifi").get<UserDeviceConnectivityWifiConfig>();
    }
    if (jsonObject.contains("ethernet")) {
        value.ethernet = jsonObject.at("ethernet").get<UserDeviceConnectivityEthernetConfig>();
    }
    if (jsonObject.contains("bluetooth")) {
        value.bluetooth = jsonObject.at("bluetooth").get<UserDeviceConnectivityBluetoothConfig>();
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

inline void from_json(const json& jsonObject, UserDeviceConfig& value) {
    if (jsonObject.contains("name")) {
        value.name = jsonObject.at("name").get<std::string>();
    }
    if (jsonObject.contains("model")) {
        value.model = jsonObject.at("model").get<std::string>();
    }
    if (jsonObject.contains("version")) {
        value.version = jsonObject.at("version").get<std::string>();
    }
    if (jsonObject.contains("serial_number")) {
        value.serial_number = jsonObject.at("serial_number").get<std::string>();
    }
    if (jsonObject.contains("manufacturer")) {
        value.manufacturer = jsonObject.at("manufacturer").get<std::string>();
    }
    if (jsonObject.contains("timezone")) {
        value.timezone = jsonObject.at("timezone").get<std::string>();
    }
    if (jsonObject.contains("timestamp")) {
        value.timestamp = jsonObject.at("timestamp").get<std::string>();
    }
    if (jsonObject.contains("connectivity")) {
        value.connectivity = jsonObject.at("connectivity").get<UserDeviceConnectivityConfig>();
    }
}

}  // namespace sst::config::domain
