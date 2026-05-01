#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "domain/common/models/pixel-format.hpp"

namespace sst::db {

enum class CameraWhiteBalance : uint8_t { kAuto = 0, kManual = 1 };
enum class CameraFocus : uint8_t { kAuto = 0, kManual = 1 };

struct CameraConfig {
    static constexpr int32_t kDefaultExposure = 100;
    static constexpr float kDefaultGain = 1.0F;
    static constexpr int32_t kDefaultWidth = 1920;
    static constexpr int32_t kDefaultHeight = 1080;
    static constexpr int32_t kDefaultFps = 60;
    static constexpr int32_t kDefaultEventClipPreSeconds = 60;
    static constexpr int32_t kDefaultEventClipPostSeconds = 60;

    int64_t user_id{0};
    int32_t exposure{kDefaultExposure};
    float gain{kDefaultGain};
    CameraWhiteBalance white_balance{CameraWhiteBalance::kAuto};
    CameraFocus focus{CameraFocus::kAuto};
    int32_t width{kDefaultWidth};
    int32_t height{kDefaultHeight};
    sst::common::PixelFormat format{sst::common::PixelFormat::NV12};
    int32_t fps{kDefaultFps};
    int32_t event_clip_pre_seconds{kDefaultEventClipPreSeconds};
    int32_t event_clip_post_seconds{kDefaultEventClipPostSeconds};
};

struct CameraCalibration {
    static constexpr std::size_t kIntrinsicMatrixSize = 9;
    static constexpr std::size_t kDistortionCoefficientsSize = 5;

    int64_t id{0};
    int32_t camera_id{0};
    std::array<float, kIntrinsicMatrixSize> intrinsic_matrix{};
    std::array<float, kDistortionCoefficientsSize> distortion_coefficients{};
    std::string calibrated_at;
};

}  // namespace sst::db
