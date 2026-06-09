#pragma once
#include <nlohmann/json.hpp>

#include "adapters/config/json/json-opt.hpp"
#include "domain/config/models/wifi-direct.hpp"

namespace sst::config {

using nlohmann::json;

inline void from_json(const json& jsonObject, WifiDirectData& values) {
    json_get_opt(jsonObject, "enabled", values.enabled);
    json_get_opt(jsonObject, "ssid", values.ssid);
    json_get_opt(jsonObject, "passphrase", values.passphrase);
    json_get_opt(jsonObject, "channel", values.channel);
    json_get_opt(jsonObject, "ip_address", values.ip_address);
}

}  // namespace sst::config
