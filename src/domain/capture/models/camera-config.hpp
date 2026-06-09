#pragma once

#include <cstdint>

#include "domain/common/models/pixel-format.hpp"

namespace sst::capture {

enum class CameraWhiteBalance : uint8_t { kAuto = 0, kManual = 1 };
enum class CameraFocus : uint8_t { kAuto = 0, kManual = 1 };

// Per-sensor capture configuration for an IMX477. This is device hardware
// config (sensor mode), not business data, so it survives the app-as-source-of-
// truth refactor. Sourced from a compiled-in default today; lens calibration
// lives separately in calibration.json.
struct CameraConfig {
    static constexpr int32_t kDefaultExposure = 100;
    static constexpr float kDefaultGain = 1.0F;
    static constexpr int32_t kDefaultWidth = 1920;
    static constexpr int32_t kDefaultHeight = 1080;
    static constexpr int32_t kDefaultFps = 60;

    int32_t exposure{kDefaultExposure};
    float gain{kDefaultGain};
    CameraWhiteBalance white_balance{CameraWhiteBalance::kAuto};
    CameraFocus focus{CameraFocus::kAuto};
    int32_t width{kDefaultWidth};
    int32_t height{kDefaultHeight};
    sst::common::PixelFormat format{sst::common::PixelFormat::NV12};
    int32_t fps{kDefaultFps};
};

}  // namespace sst::capture
