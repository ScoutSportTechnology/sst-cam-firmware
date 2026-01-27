#pragma once

#include <cstdint>
#include <string>
#include <string_view>

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
inline auto FromString(std::string_view format_str) -> PixelFormat {
    if (format_str == "NV12") {
        return PixelFormat::NV12;
    }
    if (format_str == "I420") {
        return PixelFormat::I420;
    }
    if (format_str == "YUYV") {
        return PixelFormat::YUYV;
    }
    if (format_str == "YUY2") {
        return PixelFormat::YUYV;
    }
    if (format_str == "RGB8") {
        return PixelFormat::RGB8;
    }
    if (format_str == "BGR8") {
        return PixelFormat::BGR8;
    }
    if (format_str == "RGBA8") {
        return PixelFormat::RGBA8;
    }
    if (format_str == "BGRA8") {
        return PixelFormat::BGRA8;
    }
    if (format_str == "GRAY8") {
        return PixelFormat::GRAY8;
    }
    return PixelFormat::UNKNOWN;
}
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
        default:
            return "UNKNOWN";
    }
}
}  // namespace sst::common::domain