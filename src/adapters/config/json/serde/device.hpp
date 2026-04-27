#pragma once
#include <nlohmann/json.hpp>

#include "adapters/config/json/json-opt.hpp"
#include "domain/config/models/device.hpp"

namespace sst::config {

using nlohmann::json;

inline void from_json(const json& jsonObject, DeviceStaticIpData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "ip_address", values.ip_address);
    json_get_opt(jsonObject, "subnet_mask", values.subnet_mask);
    json_get_opt(jsonObject, "gateway", values.gateway);
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiClientData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "wifi_ssid", values.wifi_ssid);
    json_get_opt(jsonObject, "wifi_password", values.wifi_password);
    json_get_opt(jsonObject, "static_ip", values.static_ip);
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiAccessPointData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "ssid", values.ssid);
    json_get_opt(jsonObject, "password", values.password);
}

inline void from_json(const json& jsonObject, DeviceConnectivityWifiData& values) {
    json_get_opt(jsonObject, "client", values.client);
    json_get_opt(jsonObject, "access_point", values.access_point);
}

inline void from_json(const json& jsonObject, DeviceConnectivityEthernetData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "static_ip", values.static_ip);
}

inline void from_json(const json& jsonObject, DeviceConnectivityBluetoothData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "name", values.name);
    json_get_opt(jsonObject, "password", values.password);
}

inline void from_json(const json& jsonObject, DeviceConnectivityData& values) {
    json_get_opt(jsonObject, "wifi", values.wifi);
    json_get_opt(jsonObject, "ethernet", values.ethernet);
    json_get_opt(jsonObject, "bluetooth", values.bluetooth);
}

inline void from_json(const json& jsonObject, DeviceData& values) {
    json_get_opt(jsonObject, "name", values.name);
    json_get_opt(jsonObject, "model", values.model);
    json_get_opt(jsonObject, "version", values.version);
    json_get_opt(jsonObject, "serial_number", values.serial_number);
    json_get_opt(jsonObject, "manufacturer", values.manufacturer);
    json_get_opt(jsonObject, "timezone", values.timezone);
    json_get_opt(jsonObject, "timestamp", values.timestamp);
    json_get_opt(jsonObject, "connectivity", values.connectivity);
}

}  // namespace sst::config
