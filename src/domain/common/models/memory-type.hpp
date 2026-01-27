#pragma once

#include <cstdint>

namespace sst::common::domain {
enum class MemoryType : std::uint8_t {
    CPU,
    GPU,
    NVMM,
};
}