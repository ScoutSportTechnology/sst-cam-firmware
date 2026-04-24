#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/calibration.hpp"

namespace sst::config {

using nlohmann::json;

inline void to_json(json& jsonObject, const CalibrationCameraDeviceData& values) {
    jsonObject = json::object();
    if (values.id) {
        jsonObject["id"] = *values.id;
    }
    if (values.intrinsic_matrix) {
        jsonObject["intrinsic_matrix"] = *values.intrinsic_matrix;
    }
    if (values.distortion_coefficients) {
        jsonObject["distortion_coefficients"] = *values.distortion_coefficients;
    }
    if (values.last_calibration_date) {
        jsonObject["last_calibration_date"] = *values.last_calibration_date;
    }
}

inline void to_json(json& jsonObject, const CalibrationCamerasData& values) {
    jsonObject = json::object();
    if (values.exposure) {
        jsonObject["exposure"] = *values.exposure;
    }
    if (values.gain) {
        jsonObject["gain"] = *values.gain;
    }
    if (values.white_balance) {
        jsonObject["white_balance"] = *values.white_balance;
    }
    if (values.focus) {
        jsonObject["focus"] = *values.focus;
    }
    if (values.width) {
        jsonObject["width"] = *values.width;
    }
    if (values.height) {
        jsonObject["height"] = *values.height;
    }
    if (values.format) {
        jsonObject["format"] = *values.format;
    }
    if (values.fps) {
        jsonObject["fps"] = *values.fps;
    }
    if (values.device) {
        jsonObject["device"] = *values.device;
    }
}

inline void to_json(json& jsonObject, const CalibrationMicrophoneDeviceData& values) {
    jsonObject = json::object();
    if (values.id) {
        jsonObject["id"] = *values.id;
    }
    if (values.sensitivity) {
        jsonObject["sensitivity"] = *values.sensitivity;
    }
    if (values.last_calibration_date) {
        jsonObject["last_calibration_date"] = *values.last_calibration_date;
    }
}

inline void to_json(json& jsonObject, const CalibrationMicrophonesData& values) {
    jsonObject = json::object();
    if (values.noise_reduction) {
        jsonObject["noise_reduction"] = *values.noise_reduction;
    }
    if (values.device) {
        jsonObject["device"] = *values.device;
    }
}

inline void to_json(json& jsonObject, const CalibrationData& values) {
    jsonObject = json::object();
    if (values.cameras) {
        jsonObject["cameras"] = *values.cameras;
    }
    if (values.microphones) {
        jsonObject["microphones"] = *values.microphones;
    }
}

}  // namespace sst::config
