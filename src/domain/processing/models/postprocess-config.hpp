#pragma once

#include <cstdint>

#include "domain/common/models/pixel-format.hpp"

namespace sst::processing {

// Hard-coded defaults. Not loaded from JSON. Override via ctor only in tests.
struct PostprocessConfig {
    std::uint32_t output_width{1280};
    std::uint32_t output_height{720};
    sst::common::PixelFormat output_format{sst::common::PixelFormat::BGR8};
};

}  // namespace sst::processing
