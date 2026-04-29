#pragma once

#include <cstdint>

#include "domain/processing/models/color-mode.hpp"

namespace sst::processing {

// Hard-coded defaults. Not loaded from JSON. Override via ctor only in tests.
struct PreprocessConfig {
    std::uint32_t ai_width{640};
    std::uint32_t ai_height{640};
    ColorMode ai_color_mode{ColorMode::Grayscale};
    std::uint8_t binary_threshold{127};  // used iff ai_color_mode == Binary
};

}  // namespace sst::processing
