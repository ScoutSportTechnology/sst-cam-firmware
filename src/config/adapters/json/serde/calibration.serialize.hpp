#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/calibration_config.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void to_json(json& jsonObject, const DefaultCalibrationDevicesCameraConfig& value) {
    jsonObject = json{{"last_calibration_date", value.last_calibration_date},
                      {"id", value.id},
                      {"exposure", value.exposure},
                      {"gain", value.gain},
                      {"white_balance", value.white_balance},
                      {"focus", value.focus},
                      {"intrinsic_matrix", value.intrinsic_matrix},
                      {"distortion_coefficients", value.distortion_coefficients}};
}

inline void to_json(json& jsonObject, const DefaultCalibrationDevicesMicrophoneConfig& value) {
    jsonObject = json{{"last_calibration_date", value.last_calibration_date},
                      {"id", value.id},
                      {"sensitivity", value.sensitivity},
                      {"noise_reduction", value.noise_reduction}};
}

inline void to_json(json& jsonObject, const DefaultCalibrationDevicesConfig& value) {
    jsonObject = json{{"camera", value.camera}, {"microphone", value.microphone}};
}

inline void to_json(json& jsonObject, const DefaultCalibrationConfig& value) {
    jsonObject = json{{"devices", value.devices}};
}

inline void to_json(json& jsonObject, const UserCalibrationDevicesCameraConfig& value) {
    jsonObject = json::object();

    if (value.last_calibration_date.has_value()) {
        jsonObject["last_calibration_date"] = value.last_calibration_date.value();
    }
    if (value.id.has_value()) {
        jsonObject["id"] = value.id.value();
    }
    if (value.exposure.has_value()) {
        jsonObject["exposure"] = value.exposure.value();
    }
    if (value.gain.has_value()) {
        jsonObject["gain"] = value.gain.value();
    }
    if (value.white_balance.has_value()) {
        jsonObject["white_balance"] = value.white_balance.value();
    }
    if (value.focus.has_value()) {
        jsonObject["focus"] = value.focus.value();
    }
    if (value.intrinsic_matrix.has_value()) {
        jsonObject["intrinsic_matrix"] = value.intrinsic_matrix.value();
    }
    if (value.distortion_coefficients.has_value()) {
        jsonObject["distortion_coefficients"] = value.distortion_coefficients.value();
    }
}

inline void to_json(json& jsonObject, const UserCalibrationDevicesMicrophoneConfig& value) {
    jsonObject = json::object();

    if (value.last_calibration_date.has_value()) {
        jsonObject["last_calibration_date"] = value.last_calibration_date.value();
    }
    if (value.id.has_value()) {
        jsonObject["id"] = value.id.value();
    }
    if (value.sensitivity.has_value()) {
        jsonObject["sensitivity"] = value.sensitivity.value();
    }
    if (value.noise_reduction.has_value()) {
        jsonObject["noise_reduction"] = value.noise_reduction.value();
    }
}

inline void to_json(json& jsonObject, const UserCalibrationDevicesConfig& value) {
    jsonObject = json{{"camera", value.camera}, {"microphone", value.microphone}};
}

inline void to_json(json& jsonObject, const UserCalibrationConfig& value) {
    jsonObject = json::object();

    if (value.devices.has_value()) {
        jsonObject["devices"] = value.devices.value();
    }
}

}  // namespace sst::config::domain
