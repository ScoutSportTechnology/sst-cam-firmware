#pragma once

#include <cstdint>
#include <string>

namespace sst::processing {

enum class ColorMode : std::uint8_t {
    Grayscale = 0,
    Binary,
    RGB,
};

inline auto ToString(ColorMode mode) -> std::string {
    switch (mode) {
        case ColorMode::Grayscale:
            return "Grayscale";
        case ColorMode::Binary:
            return "Binary";
        case ColorMode::RGB:
            return "RGB";
    }
    return "";
}

}  // namespace sst::processing
