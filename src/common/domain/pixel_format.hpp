#pragma once

#include <cstdint>

namespace sst::common::domain {
enum class PixelFormat : std::uint8_t {
    UNKNOWN,
    NV12,
    I420,
    YUYV,
    RGB8,
    BGR8,
    RGBA8,
    BGRA8,
    GRAY8,
};
}