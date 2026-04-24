#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/calibration.hpp"

namespace sst::config {

using nlohmann::json;

inline void from_json(const json& jsonObject, CalibrationCameraDeviceData& values) {
    if (jsonObject.contains("id")) {
        values.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("intrinsic_matrix")) {
        values.intrinsic_matrix = jsonObject.at("intrinsic_matrix").get<std::array<float, 9>>();
    }
    if (jsonObject.contains("distortion_coefficients")) {
        values.distortion_coefficients =
            jsonObject.at("distortion_coefficients").get<std::array<float, 5>>();
    }
    if (jsonObject.contains("last_calibration_date")) {
        values.last_calibration_date =
            jsonObject.at("last_calibration_date").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, CalibrationCamerasData& values) {
    if (jsonObject.contains("exposure")) {
        values.exposure = jsonObject.at("exposure").get<std::uint32_t>();
    }
    if (jsonObject.contains("gain")) {
        values.gain = jsonObject.at("gain").get<float>();
    }
    if (jsonObject.contains("white_balance")) {
        values.white_balance = jsonObject.at("white_balance").get<std::uint32_t>();
    }
    if (jsonObject.contains("focus")) {
        values.focus = jsonObject.at("focus").get<std::uint32_t>();
    }
    if (jsonObject.contains("width")) {
        values.width = jsonObject.at("width").get<std::uint32_t>();
    }
    if (jsonObject.contains("height")) {
        values.height = jsonObject.at("height").get<std::uint32_t>();
    }
    if (jsonObject.contains("format")) {
        values.format = jsonObject.at("format").get<std::uint32_t>();
    }
    if (jsonObject.contains("fps")) {
        values.fps = jsonObject.at("fps").get<std::uint32_t>();
    }
    if (jsonObject.contains("device") && !jsonObject.at("device").is_null()) {
        values.device = jsonObject.at("device").get<std::vector<CalibrationCameraDeviceData>>();
    }
}

inline void from_json(const json& jsonObject, CalibrationMicrophoneDeviceData& values) {
    if (jsonObject.contains("id")) {
        values.id = jsonObject.at("id").get<std::uint32_t>();
    }
    if (jsonObject.contains("sensitivity")) {
        values.sensitivity = jsonObject.at("sensitivity").get<float>();
    }
    if (jsonObject.contains("last_calibration_date")) {
        values.last_calibration_date =
            jsonObject.at("last_calibration_date").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, CalibrationMicrophonesData& values) {
    if (jsonObject.contains("noise_reduction")) {
        values.noise_reduction = jsonObject.at("noise_reduction").get<bool>();
    }
    if (jsonObject.contains("device") && !jsonObject.at("device").is_null()) {
        values.device =
            jsonObject.at("device").get<std::vector<CalibrationMicrophoneDeviceData>>();
    }
}

inline void from_json(const json& jsonObject, CalibrationData& values) {
    if (jsonObject.contains("cameras")) {
        values.cameras = jsonObject.at("cameras").get<CalibrationCamerasData>();
    }
    if (jsonObject.contains("microphones")) {
        values.microphones = jsonObject.at("microphones").get<CalibrationMicrophonesData>();
    }
}

}  // namespace sst::config
