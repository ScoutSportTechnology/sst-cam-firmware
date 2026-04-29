#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/buffer/models/buffer-policy.hpp"

namespace sst::buffer {

struct BufferStats {
    BufferPolicy policy{BufferPolicy::LatestOnly};
    std::size_t capacity{0};
    std::size_t depth{0};
    std::uint64_t pushed{0};
    std::uint64_t popped{0};
    std::uint64_t dropped{0};
};

}  // namespace sst::buffer
