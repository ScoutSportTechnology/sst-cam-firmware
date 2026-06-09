#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/overlay/models/overlay-layout.hpp"

namespace sst::overlay {

// A fully-resolved element ready to rasterize: the binding has been evaluated to
// concrete `text` (for TEXT shapes) and any template {{param}} substitution is
// already applied. The renderer needs no knowledge of bindings or templates.
struct RenderElement {
    OverlayShape shape{OverlayShape::kUnknown};
    OverlayRect bounds;
    OverlayStyle style;
    std::string text;  // resolved text for TEXT shapes; empty otherwise
};

// The flattened, z-ordered scene to draw onto one RGBA surface. Coordinates are
// in canvas space; the renderer scales canvas -> output resolution.
struct RenderScene {
    std::uint32_t canvas_width{0};
    std::uint32_t canvas_height{0};
    std::vector<RenderElement> elements;  // sorted by bounds.z ascending
};

// An owned RGBA8888 image (tightly packed; stride = width * 4).
struct RgbaImage {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t stride{0};
    std::vector<std::uint8_t> pixels;
};

}  // namespace sst::overlay
