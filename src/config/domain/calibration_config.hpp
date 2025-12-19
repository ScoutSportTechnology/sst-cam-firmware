#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "config/domain/config_files.hpp"

namespace sst::config::domain {

struct CalibrationCameraData {
    std::optional<std::uint32_t> id{std::nullopt};
    std::optional<std::uint32_t> exposure{std::nullopt};
    std::optional<float> gain{std::nullopt};
    std::optional<std::string> white_balance{std::nullopt};
    std::optional<std::string> focus{std::nullopt};
    std::optional<std::uint32_t> width{std::nullopt};
    std::optional<std::uint32_t> height{std::nullopt};
    std::optional<std::string> format{std::nullopt};
    std::optional<std::uint32_t> fps{std::nullopt};
    std::optional<std::array<float, 9>> intrinsic_matrix{std::nullopt};
    std::optional<std::array<float, 5>> distortion_coefficients{std::nullopt};
    std::optional<std::string> last_calibration_date{std::nullopt};
};

struct CalibrationMicrophoneData {
    std::optional<std::uint32_t> id{std::nullopt};
    std::optional<float> sensitivity{std::nullopt};
    std::optional<bool> noise_reduction{std::nullopt};
    std::optional<std::string> last_calibration_date{std::nullopt};
};

struct CalibrationDevicesData {
    std::optional<std::vector<CalibrationCameraData>> camera;
    std::optional<std::vector<CalibrationMicrophoneData>> microphone;
};

struct CalibrationData {
    std::optional<CalibrationDevicesData> devices{std::nullopt};
};

using CalibrationConfig = ConfigFiles<CalibrationData, CalibrationData>;

}  // namespace sst::config::domain
