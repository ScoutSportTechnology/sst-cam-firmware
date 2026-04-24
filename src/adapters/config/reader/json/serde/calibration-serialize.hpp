#pragma once
#include <nlohmann/json.hpp>

#include "adapters/config/json-opt.hpp"
#include "domain/config/models/calibration.hpp"

namespace sst::config {

using nlohmann::json;

inline void to_json(json& jsonObject, const CalibrationCameraDeviceData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "id", values.id);
    json_set_opt(jsonObject, "intrinsic_matrix", values.intrinsic_matrix);
    json_set_opt(jsonObject, "distortion_coefficients", values.distortion_coefficients);
    json_set_opt(jsonObject, "last_calibration_date", values.last_calibration_date);
}

inline void to_json(json& jsonObject, const CalibrationCamerasData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "exposure", values.exposure);
    json_set_opt(jsonObject, "gain", values.gain);
    json_set_opt(jsonObject, "white_balance", values.white_balance);
    json_set_opt(jsonObject, "focus", values.focus);
    json_set_opt(jsonObject, "width", values.width);
    json_set_opt(jsonObject, "height", values.height);
    json_set_opt(jsonObject, "format", values.format);
    json_set_opt(jsonObject, "fps", values.fps);
    json_set_opt(jsonObject, "device", values.device);
}

inline void to_json(json& jsonObject, const CalibrationMicrophoneDeviceData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "id", values.id);
    json_set_opt(jsonObject, "sensitivity", values.sensitivity);
    json_set_opt(jsonObject, "last_calibration_date", values.last_calibration_date);
}

inline void to_json(json& jsonObject, const CalibrationMicrophonesData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "noise_reduction", values.noise_reduction);
    json_set_opt(jsonObject, "device", values.device);
}

inline void to_json(json& jsonObject, const CalibrationData& values) {
    jsonObject = json::object();
    json_set_opt(jsonObject, "cameras", values.cameras);
    json_set_opt(jsonObject, "microphones", values.microphones);
}

}  // namespace sst::config
