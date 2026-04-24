#pragma once
#include <nlohmann/json.hpp>

#include "adapters/config/json-opt.hpp"
#include "domain/config/models/calibration.hpp"

namespace sst::config {

using nlohmann::json;

inline void from_json(const json& jsonObject, CalibrationCameraDeviceData& values) {
    json_get_opt(jsonObject, "id", values.id);
    json_get_opt(jsonObject, "intrinsic_matrix", values.intrinsic_matrix);
    json_get_opt(jsonObject, "distortion_coefficients", values.distortion_coefficients);
    json_get_opt(jsonObject, "last_calibration_date", values.last_calibration_date);
}

inline void from_json(const json& jsonObject, CalibrationCamerasData& values) {
    json_get_opt(jsonObject, "exposure", values.exposure);
    json_get_opt(jsonObject, "gain", values.gain);
    json_get_opt(jsonObject, "white_balance", values.white_balance);
    json_get_opt(jsonObject, "focus", values.focus);
    json_get_opt(jsonObject, "width", values.width);
    json_get_opt(jsonObject, "height", values.height);
    json_get_opt(jsonObject, "format", values.format);
    json_get_opt(jsonObject, "fps", values.fps);
    if (jsonObject.contains("device") && !jsonObject.at("device").is_null()) {
        values.device = jsonObject.at("device").get<std::vector<CalibrationCameraDeviceData>>();
    }
}

inline void from_json(const json& jsonObject, CalibrationMicrophoneDeviceData& values) {
    json_get_opt(jsonObject, "id", values.id);
    json_get_opt(jsonObject, "sensitivity", values.sensitivity);
    json_get_opt(jsonObject, "last_calibration_date", values.last_calibration_date);
}

inline void from_json(const json& jsonObject, CalibrationMicrophonesData& values) {
    json_get_opt(jsonObject, "noise_reduction", values.noise_reduction);
    if (jsonObject.contains("device") && !jsonObject.at("device").is_null()) {
        values.device =
            jsonObject.at("device").get<std::vector<CalibrationMicrophoneDeviceData>>();
    }
}

inline void from_json(const json& jsonObject, CalibrationData& values) {
    json_get_opt(jsonObject, "cameras", values.cameras);
    json_get_opt(jsonObject, "microphones", values.microphones);
}

}  // namespace sst::config
