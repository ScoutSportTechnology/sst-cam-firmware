#pragma once

#include <cstdint>

namespace sst::common {
enum class Rotation : std::uint8_t {
    DEGREE_0,
    DEGREE_90,
    DEGREE_180,
    DEGREE_270

};
}