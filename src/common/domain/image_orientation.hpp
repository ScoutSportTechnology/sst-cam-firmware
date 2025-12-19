#pragma once

#include <cstdint>

namespace sst::common::domain {
enum class ImageOrientation : std::uint8_t {
    NORMAL = 0,
    ROTATE_90 = 1,
    ROTATE_180 = 2,
    ROTATE_270 = 3,
};
}