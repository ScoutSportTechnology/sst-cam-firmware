#pragma once
#include <nlohmann/json.hpp>

#include "adapters/config/json-opt.hpp"
#include "domain/config/models/device.hpp"

namespace sst::config {

using nlohmann::json;

inline void to_json(json& jsonObject, const DeviceStaticIpData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "enabled", values.enabled);
    json_set_opt(jsonObject, "ip_address", values.ip_address);
    json_set_opt(jsonObject, "subnet_mask", values.subnet_mask);
    json_set_opt(jsonObject, "gateway", values.gateway);
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiClientData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "enabled", values.enabled);
    json_set_opt(jsonObject, "wifi_ssid", values.wifi_ssid);
    json_set_opt(jsonObject, "wifi_password", values.wifi_password);
    json_set_opt(jsonObject, "static_ip", values.static_ip);
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiAccessPointData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "enabled", values.enabled);
    json_set_opt(jsonObject, "ssid", values.ssid);
    json_set_opt(jsonObject, "password", values.password);
}

inline void to_json(json& jsonObject, const DeviceConnectivityWifiData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "client", values.client);
    json_set_opt(jsonObject, "access_point", values.access_point);
}

inline void to_json(json& jsonObject, const DeviceConnectivityEthernetData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "enabled", values.enabled);
    json_set_opt(jsonObject, "static_ip", values.static_ip);
}

inline void to_json(json& jsonObject, const DeviceConnectivityBluetoothData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "enabled", values.enabled);
    json_set_opt(jsonObject, "name", values.name);
    json_set_opt(jsonObject, "password", values.password);
}

inline void to_json(json& jsonObject, const DeviceConnectivityData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "wifi", values.wifi);
    json_set_opt(jsonObject, "ethernet", values.ethernet);
    json_set_opt(jsonObject, "bluetooth", values.bluetooth);
}

inline void to_json(json& jsonObject, const DeviceData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "name", values.name);
    json_set_opt(jsonObject, "model", values.model);
    json_set_opt(jsonObject, "version", values.version);
    json_set_opt(jsonObject, "serial_number", values.serial_number);
    json_set_opt(jsonObject, "manufacturer", values.manufacturer);
    json_set_opt(jsonObject, "timezone", values.timezone);
    json_set_opt(jsonObject, "timestamp", values.timestamp);
    json_set_opt(jsonObject, "connectivity", values.connectivity);
}

}  // namespace sst::config
