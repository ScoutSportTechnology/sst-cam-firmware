#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/profile.hpp"

namespace sst::config{
using nlohmann::json;

inline void from_json(const json& jsonObject, ProfileData& values) {
    if (jsonObject.contains("calibration")) {
        values.calibration = jsonObject.at("calibration").get<bool>();
    }

    if (jsonObject.contains("device")) {
        values.device = jsonObject.at("device").get<bool>();
    }

    if (jsonObject.contains("stream")) {
        values.stream = jsonObject.at("stream").get<bool>();
    }

    if (jsonObject.contains("storage")) {
        values.storage = jsonObject.at("storage").get<bool>();
    }
}

}  // namespace sst::config
