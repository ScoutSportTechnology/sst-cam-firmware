#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/profile_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const ProfileData& values) {
    jsonObject = json::object();

    if (values.calibration.has_value()) {
        jsonObject["calibration"] = *values.calibration;
    }

    if (values.device.has_value()) {
        jsonObject["device"] = *values.device;
    }

    if (values.stream.has_value()) {
        jsonObject["stream"] = *values.stream;
    }

    if (values.storage.has_value()) {
        jsonObject["storage"] = *values.storage;
    }
}
}  // namespace sst::config::domain