#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/overlay/models/overlay-enums.hpp"

namespace sst::overlay {

// Position + layer of an element in logical canvas space (top-left origin).
struct OverlayRect {
    float x1{0.0F};
    float y1{0.0F};
    float x2{0.0F};
    float y2{0.0F};
    std::uint32_t z{0};
};

// Visual style. Colors are "#RRGGBB" hex; empty fill = transparent.
struct OverlayStyle {
    std::string fill_color;
    std::string text_color;
    float opacity{1.0F};
    float corner_radius{0.0F};
    std::string font_family;
    float font_size{0.0F};
    TextAlign text_align{TextAlign::kLeft};
    FontWeight font_weight{FontWeight::kNormal};
    std::string static_text;
};

// A single visual element.
struct OverlayElement {
    std::string id;
    OverlayShape shape{OverlayShape::kUnknown};
    OverlayRect bounds;
    OverlayStyle style;
    OverlayBinding binding{OverlayBinding::kStatic};
    bool visible{true};
};

// An event-triggered template: shown when a BannerEvent(event_type) arrives,
// hidden after duration_ms. Element static_text may carry {{param}} placeholders.
struct OverlayTemplate {
    std::string event_type;
    std::uint32_t duration_ms{0};
    std::vector<OverlayElement> elements;
};

// The full declarative overlay design pushed by the app.
struct OverlayLayout {
    std::uint32_t canvas_width{0};
    std::uint32_t canvas_height{0};
    std::vector<OverlayElement> elements;
    std::vector<OverlayTemplate> templates;
};

}  // namespace sst::overlay
