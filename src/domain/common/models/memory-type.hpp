#pragma once

#include <cstdint>

namespace sst::common {
enum class MemoryType : std::uint8_t {
    CPU,
    GPU,
    NVMM,
};
}