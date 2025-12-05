#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "config/domain/config_files.hpp"

namespace domain::config {
struct DefaultCalibrationDevicesCameraConfig {
    std::string last_calibration_date;
    std::uint32_t id{0};
    std::uint32_t exposure{0};
    float gain{0.0f};
    std::string white_balance;
    std::string focus;
    std::array<float, 9> intrinsic_matrix{};
    std::array<float, 5> distortion_coefficients{};
};
struct DefaultCalibrationDevicesMicrophoneConfig {
    std::string last_calibration_date;
    std::uint32_t id{0};
    float sensitivity{0.0f};
    bool noise_reduction{false};
};
struct DefaultCalibrationDevicesConfig {
    std::vector<DefaultCalibrationDevicesCameraConfig> camera;
    std::vector<DefaultCalibrationDevicesMicrophoneConfig> microphone;
};
struct DefaultCalibrationConfig {
    DefaultCalibrationDevicesConfig devices;
};

struct UserCalibrationDevicesCameraConfig {
    std::optional<std::string> last_calibration_date;
    std::optional<std::uint32_t> id;
    std::optional<std::uint32_t> exposure;
    std::optional<float> gain;
    std::optional<std::string> white_balance;
    std::optional<std::string> focus;
    std::optional<std::array<float, 9>> intrinsic_matrix;
    std::optional<std::array<float, 5>> distortion_coefficients;
};
struct UserCalibrationDevicesMicrophoneConfig {
    std::optional<std::string> last_calibration_date;
    std::optional<std::uint32_t> id;
    std::optional<float> sensitivity;
    std::optional<bool> noise_reduction;
};
struct UserCalibrationDevicesConfig {
    std::vector<UserCalibrationDevicesCameraConfig> camera;
    std::vector<UserCalibrationDevicesMicrophoneConfig> microphone;
};
struct UserCalibrationConfig {
    std::optional<UserCalibrationDevicesConfig> devices;
};

using CalibrationConfig =
    ConfigFiles<DefaultCalibrationConfig, UserCalibrationConfig>;

}  // namespace domain::config
