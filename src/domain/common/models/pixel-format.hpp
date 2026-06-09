#pragma once

#include <cstdint>
#include <string>

namespace sst::common {

enum class PixelFormat : std::uint8_t {
    NV12 = 0,
    I420,
    YUYV,
    RGB8,
    BGR8,
    RGBA8,
    BGRA8,
    GRAY8,
};

inline auto ToString(PixelFormat format) -> std::string {
    switch (format) {
        case PixelFormat::NV12:
            return "NV12";
        case PixelFormat::I420:
            return "I420";
        case PixelFormat::YUYV:
            return "YUYV";
        case PixelFormat::RGB8:
            return "RGB8";
        case PixelFormat::BGR8:
            return "BGR8";
        case PixelFormat::RGBA8:
            return "RGBA8";
        case PixelFormat::BGRA8:
            return "BGRA8";
        case PixelFormat::GRAY8:
            return "GRAY8";
    }
    return "";
}

}  // namespace sst::common
