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

inline void to_json(json& jsonObject, const DefaultCalibrationDevicesMicrophoneConfig& value) {
    jsonObject = json{{"last_calibration_date", value.last_calibration_date},
                      {"id", value.id},
                      {"sensitivity", value.sensitivity},
                      {"noise_reduction", value.noise_reduction}};
}

inline void from_json(const json& jsonObject, DefaultCalibrationDevicesMicrophoneConfig& value) {
    jsonObject.at("last_calibration_date").get_to(value.last_calibration_date);
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("sensitivity").get_to(value.sensitivity);
    jsonObject.at("noise_reduction").get_to(value.noise_reduction);
}

inline void to_json(json& jsonObject, const DefaultCalibrationDevicesConfig& value) {
    jsonObject = json{{"camera", value.camera}, {"microphone", value.microphone}};
}

inline void from_json(const json& jsonObject, DefaultCalibrationDevicesConfig& value) {
    jsonObject.at("camera").get_to(value.camera);
    jsonObject.at("microphone").get_to(value.microphone);
}

inline void to_json(json& jsonObject, const DefaultCalibrationConfig& value) {
    jsonObject = json{{"devices", value.devices}};
}

inline void from_json(const json& jsonObject, DefaultCalibrationConfig& value) {
    jsonObject.at("devices").get_to(value.devices);
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

inline void to_json(json& jsonObject, const UserCalibrationDevicesConfig& value) {
    jsonObject = json{{"camera", value.camera}, {"microphone", value.microphone}};
}

inline void from_json(const json& jsonObject, UserCalibrationDevicesConfig& value) {
    if (jsonObject.contains("camera")) {
        jsonObject.at("camera").get_to(value.camera);
    }
    if (jsonObject.contains("microphone")) {
        jsonObject.at("microphone").get_to(value.microphone);
    }
}

inline void to_json(json& jsonObject, const UserCalibrationConfig& value) {
    jsonObject = json::object();

    if (value.devices.has_value()) {
        jsonObject["devices"] = value.devices.value();
    }
}

inline void from_json(const json& jsonObject, UserCalibrationConfig& value) {
    if (jsonObject.contains("devices") && !jsonObject.at("devices").is_null()) {
        value.devices = jsonObject.at("devices").get<UserCalibrationDevicesConfig>();
    }
}

}  // namespace sst::config::domain
