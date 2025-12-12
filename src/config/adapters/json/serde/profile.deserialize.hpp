#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/profile_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, DefaultProfileConfig& value) {
    jsonObject.at("calibration").get_to(value.calibration);
    jsonObject.at("device").get_to(value.device);
    jsonObject.at("stream").get_to(value.stream);
    jsonObject.at("storage").get_to(value.storage);
}

inline void from_json(const json& jsonObject, UserProfileConfig& value) {
    if (jsonObject.contains("calibration")) {
        value.calibration = jsonObject.at("calibration").get<bool>();
    }
    if (jsonObject.contains("device")) {
        value.device = jsonObject.at("device").get<bool>();
    }
    if (jsonObject.contains("stream")) {
        value.stream = jsonObject.at("stream").get<bool>();
    }
    if (jsonObject.contains("storage")) {
        value.storage = jsonObject.at("storage").get<bool>();
    }
}

}  // namespace sst::config::domain
