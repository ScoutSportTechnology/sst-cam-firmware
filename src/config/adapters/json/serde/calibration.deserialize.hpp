#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/calibration_config.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void from_json(const json& jsonObject, CalibrationCameraData& values) {
    if (jsonObject.contains("last_calibration_date")) {
        values.last_calibration_date = jsonObject.at("last_calibration_date").get<std::string>();
    }
    if (jsonObject.contains("id")) {
        values.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("exposure")) {
        values.exposure = jsonObject.at("exposure").get<std::uint32_t>();
    }
    if (jsonObject.contains("gain")) {
        values.gain = jsonObject.at("gain").get<float>();
    }
    if (jsonObject.contains("white_balance")) {
        values.white_balance = jsonObject.at("white_balance").get<std::string>();
    }
    if (jsonObject.contains("focus")) {
        values.focus = jsonObject.at("focus").get<std::string>();
    }
    if (jsonObject.contains("intrinsic_matrix")) {
        values.intrinsic_matrix = jsonObject.at("intrinsic_matrix").get<std::array<float, 9>>();
    }
    if (jsonObject.contains("distortion_coefficients")) {
        values.distortion_coefficients =
            jsonObject.at("distortion_coefficients").get<std::array<float, 5>>();
    }
}

inline void from_json(const json& jsonObject, CalibrationMicrophoneData& values) {
    if (jsonObject.contains("last_calibration_date")) {
        values.last_calibration_date = jsonObject.at("last_calibration_date").get<std::string>();
    }
    if (jsonObject.contains("id")) {
        values.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("sensitivity")) {
        values.sensitivity = jsonObject.at("sensitivity").get<float>();
    }
    if (jsonObject.contains("noise_reduction")) {
        values.noise_reduction = jsonObject.at("noise_reduction").get<bool>();
    }
}

inline void from_json(const json& jsonObject, CalibrationDevicesData& values) {
    if (jsonObject.contains("camera")) {
        jsonObject.at("camera").get_to(values.camera);
    }
    if (jsonObject.contains("microphone")) {
        jsonObject.at("microphone").get_to(values.microphone);
    }
}

inline void from_json(const json& jsonObject, CalibrationData& values) {
    if (jsonObject.contains("devices")) {
        values.devices = jsonObject.at("devices").get<CalibrationDevicesData>();
    }
}

}  // namespace sst::config::domain
