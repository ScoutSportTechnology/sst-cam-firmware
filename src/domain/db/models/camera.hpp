#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace sst::db {

enum class CameraWhiteBalance : uint8_t { kAuto = 0, kManual = 1 };
enum class CameraFocus : uint8_t { kAuto = 0, kManual = 1 };

// Integer values match pixel-format.hpp PixelFormat enum order for direct cast
enum class CameraFormat : uint8_t {
    kYUV422 = 0,
    kNV12 = 1,
    kI420 = 2,
    kRGB8 = 3,
    kBGR8 = 4,
    kRGBA8 = 5,
    kBGRA8 = 6,
    kGRAY8 = 7,
};

struct CameraConfig {
    static constexpr int32_t kDefaultExposure = 100;
    static constexpr float kDefaultGain = 1.0F;
    static constexpr int32_t kDefaultWidth = 1920;
    static constexpr int32_t kDefaultHeight = 1080;
    static constexpr int32_t kDefaultFps = 60;

    int64_t user_id{0};
    int32_t exposure{kDefaultExposure};
    float gain{kDefaultGain};
    CameraWhiteBalance white_balance{CameraWhiteBalance::kAuto};
    CameraFocus focus{CameraFocus::kAuto};
    int32_t width{kDefaultWidth};
    int32_t height{kDefaultHeight};
    CameraFormat format{CameraFormat::kYUV422};
    int32_t fps{kDefaultFps};
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
