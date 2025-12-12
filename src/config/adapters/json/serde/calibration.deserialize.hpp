#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/calibration_config.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void from_json(const json& jsonObject, DefaultCalibrationDevicesCameraConfig& value) {
    jsonObject.at("last_calibration_date").get_to(value.last_calibration_date);
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("exposure").get_to(value.exposure);
    jsonObject.at("gain").get_to(value.gain);
    jsonObject.at("white_balance").get_to(value.white_balance);
    jsonObject.at("focus").get_to(value.focus);
    jsonObject.at("intrinsic_matrix").get_to(value.intrinsic_matrix);
    jsonObject.at("distortion_coefficients").get_to(value.distortion_coefficients);
}


inline void from_json(const json& jsonObject, DefaultCalibrationDevicesMicrophoneConfig& value) {
    jsonObject.at("last_calibration_date").get_to(value.last_calibration_date);
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("sensitivity").get_to(value.sensitivity);
    jsonObject.at("noise_reduction").get_to(value.noise_reduction);
}


inline void from_json(const json& jsonObject, DefaultCalibrationDevicesConfig& value) {
    jsonObject.at("camera").get_to(value.camera);
    jsonObject.at("microphone").get_to(value.microphone);
}


inline void from_json(const json& jsonObject, DefaultCalibrationConfig& value) {
    jsonObject.at("devices").get_to(value.devices);
}


inline void from_json(const json& jsonObject, UserCalibrationDevicesCameraConfig& value) {
    if (jsonObject.contains("last_calibration_date")) {
        value.last_calibration_date = jsonObject.at("last_calibration_date").get<std::string>();
    }
    if (jsonObject.contains("id")) {
        value.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("exposure")) {
        value.exposure = jsonObject.at("exposure").get<std::uint32_t>();
    }
    if (jsonObject.contains("gain")) {
        value.gain = jsonObject.at("gain").get<float>();
    }
    if (jsonObject.contains("white_balance")) {
        value.white_balance = jsonObject.at("white_balance").get<std::string>();
    }
    if (jsonObject.contains("focus")) {
        value.focus = jsonObject.at("focus").get<std::string>();
    }
    if (jsonObject.contains("intrinsic_matrix")) {
        value.intrinsic_matrix = jsonObject.at("intrinsic_matrix").get<std::array<float, 9>>();
    }
    if (jsonObject.contains("distortion_coefficients")) {
        value.distortion_coefficients =
            jsonObject.at("distortion_coefficients").get<std::array<float, 5>>();
    }
}


inline void from_json(const json& jsonObject, UserCalibrationDevicesMicrophoneConfig& value) {
    if (jsonObject.contains("last_calibration_date") &&
        !jsonObject.at("last_calibration_date").is_null()) {
        value.last_calibration_date = jsonObject.at("last_calibration_date").get<std::string>();
    }
    if (jsonObject.contains("id") && !jsonObject.at("id").is_null()) {
        value.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("sensitivity") && !jsonObject.at("sensitivity").is_null()) {
        value.sensitivity = jsonObject.at("sensitivity").get<float>();
    }
    if (jsonObject.contains("noise_reduction") && !jsonObject.at("noise_reduction").is_null()) {
        value.noise_reduction = jsonObject.at("noise_reduction").get<bool>();
    }
}


inline void from_json(const json& jsonObject, UserCalibrationDevicesConfig& value) {
    if (jsonObject.contains("camera")) {
        jsonObject.at("camera").get_to(value.camera);
    }
    if (jsonObject.contains("microphone")) {
        jsonObject.at("microphone").get_to(value.microphone);
    }
}


inline void from_json(const json& jsonObject, UserCalibrationConfig& value) {
    if (jsonObject.contains("devices") && !jsonObject.at("devices").is_null()) {
        value.devices = jsonObject.at("devices").get<UserCalibrationDevicesConfig>();
    }
}

}  // namespace sst::config::domain
