#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/config/models/config-files.hpp"

namespace sst::config {

struct CalibrationCameraDeviceData {
    std::optional<std::uint32_t> id{std::nullopt};
    std::optional<std::array<float, 9>> intrinsic_matrix{std::nullopt};
    std::optional<std::array<float, 5>> distortion_coefficients{std::nullopt};
    std::optional<std::string> last_calibration_date{std::nullopt};
};

struct CalibrationCamerasData {
    std::optional<std::uint32_t> exposure{std::nullopt};
    std::optional<float> gain{std::nullopt};
    std::optional<std::uint32_t> white_balance{std::nullopt};
    std::optional<std::uint32_t> focus{std::nullopt};
    std::optional<std::uint32_t> width{std::nullopt};
    std::optional<std::uint32_t> height{std::nullopt};
    std::optional<std::uint32_t> format{std::nullopt};
    std::optional<std::uint32_t> fps{std::nullopt};
    std::optional<std::vector<CalibrationCameraDeviceData>> device{std::nullopt};
};

struct CalibrationMicrophoneDeviceData {
    std::optional<std::uint32_t> id{std::nullopt};
    std::optional<float> sensitivity{std::nullopt};
    std::optional<std::string> last_calibration_date{std::nullopt};
};

struct CalibrationMicrophonesData {
    std::optional<bool> noise_reduction{std::nullopt};
    std::optional<std::vector<CalibrationMicrophoneDeviceData>> device{std::nullopt};
};

struct CalibrationData {
    std::optional<CalibrationCamerasData> cameras{std::nullopt};
    std::optional<CalibrationMicrophonesData> microphones{std::nullopt};
};

using CalibrationConfig = ConfigFiles<CalibrationData, CalibrationData>;

}  // namespace sst::config
