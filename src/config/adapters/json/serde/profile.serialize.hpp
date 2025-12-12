#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/profile_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const DefaultProfileConfig& value) {
    jsonObject = json::object();
    jsonObject["calibration"] = value.calibration;
    jsonObject["device"] = value.device;
    jsonObject["stream"] = value.stream;
    jsonObject["storage"] = value.storage;
}

inline void to_json(json& jsonObject, const UserProfileConfig& value) {
    jsonObject = json::object();
    if (value.calibration.has_value()) {
        jsonObject["calibration"] = value.calibration.value();
    }
    if (value.device.has_value()) {
        jsonObject["device"] = value.device.value();
    }
    if (value.stream.has_value()) {
        jsonObject["stream"] = value.stream.value();
    }
    if (value.storage.has_value()) {
        jsonObject["storage"] = value.storage.value();
    }
}

}  // namespace sst::config::domain
