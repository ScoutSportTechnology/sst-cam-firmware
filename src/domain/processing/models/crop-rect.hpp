#pragma once

#include <cstdint>

namespace sst::processing {

// Pixels in the source frame coordinate system. Origin top-left.
// Valid iff width > 0, height > 0, x + width <= source_width,
// y + height <= source_height.
struct CropRect {
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
};

}  // namespace sst::processing
