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

inline void from_json(const json& jsonObject, DeviceConnectivityWifiDirectData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "ssid", values.ssid);
    json_get_opt(jsonObject, "passphrase", values.passphrase);
    json_get_opt(jsonObject, "channel", values.channel);
    json_get_opt(jsonObject, "ip_address", values.ip_address);
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
    json_get_opt(jsonObject, "wifi_direct", values.wifi_direct);
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
