#include "app/overlay/overlay-proto-mapper.hpp"

#include <algorithm>

namespace sst::overlay {

namespace {

auto MapShape(sst_cam::OverlayShape s) -> OverlayShape {
    switch (s) {
        case sst_cam::SHAPE_RECT:
            return OverlayShape::kRect;
        case sst_cam::SHAPE_TEXT:
            return OverlayShape::kText;
        case sst_cam::SHAPE_CIRCLE:
            return OverlayShape::kCircle;
        default:
            return OverlayShape::kUnknown;
    }
}

auto MapBinding(sst_cam::OverlayBinding b) -> OverlayBinding {
    switch (b) {
        case sst_cam::BINDING_SCORE_A:
            return OverlayBinding::kScoreA;
        case sst_cam::BINDING_SCORE_B:
            return OverlayBinding::kScoreB;
        case sst_cam::BINDING_SCORE_VS:
            return OverlayBinding::kScoreVs;
        case sst_cam::BINDING_TEAM_A_NAME:
            return OverlayBinding::kTeamAName;
        case sst_cam::BINDING_TEAM_B_NAME:
            return OverlayBinding::kTeamBName;
        case sst_cam::BINDING_MATCH_CLOCK:
            return OverlayBinding::kMatchClock;
        case sst_cam::BINDING_PERIOD_LABEL:
            return OverlayBinding::kPeriodLabel;
        default:
            return OverlayBinding::kStatic;
    }
}

auto MapAlign(sst_cam::TextAlign a) -> TextAlign {
    switch (a) {
        case sst_cam::TEXT_ALIGN_CENTER:
            return TextAlign::kCenter;
        case sst_cam::TEXT_ALIGN_RIGHT:
            return TextAlign::kRight;
        default:
            return TextAlign::kLeft;
    }
}

auto MapWeight(sst_cam::FontWeight w) -> FontWeight {
    return w == sst_cam::FONT_WEIGHT_BOLD ? FontWeight::kBold : FontWeight::kNormal;
}

auto MapRect(const sst_cam::OverlayRect& r) -> OverlayRect {
    return OverlayRect{.x1 = r.x1(), .y1 = r.y1(), .x2 = r.x2(), .y2 = r.y2(), .z = r.z()};
}

auto MapStyle(const sst_cam::OverlayStyle& s) -> OverlayStyle {
    // proto3 `optional`: absent opacity => documented default 1.0 (opaque), not
    // the scalar zero-value (0.0 = fully transparent). See overlay-rendering.md
    // "Element defaults". Clamp to [0,1] here, the single proto->domain
    // translation point: an out-of-range alpha reaching cairo_paint_with_alpha
    // puts the cairo_t into a permanent error state, silently dropping every
    // later element in the frame.
    const float opacity = s.has_opacity() ? s.opacity() : 1.0F;
    return OverlayStyle{
        .fill_color = s.fill_color(),
        .text_color = s.text_color(),
        .opacity = std::clamp(opacity, 0.0F, 1.0F),
        .corner_radius = s.corner_radius(),
        .font_family = s.font_family(),
        .font_size = s.font_size(),
        .text_align = MapAlign(s.text_align()),
        .font_weight = MapWeight(s.font_weight()),
        .static_text = s.static_text(),
    };
}

auto MapElement(const sst_cam::OverlayElement& e) -> OverlayElement {
    return OverlayElement{
        .id = e.id(),
        .shape = MapShape(e.shape()),
        .bounds = MapRect(e.bounds()),
        .style = MapStyle(e.style()),
        .binding = MapBinding(e.binding()),
        // proto3 `optional`: absent visible => documented default true (renders),
        // not the scalar zero-value (false = hidden). See overlay-rendering.md
        // "Element defaults".
        .visible = e.has_visible() ? e.visible() : true,
    };
}

}  // namespace

auto MapLayoutFromProto(const sst_cam::OverlayLayout& proto) -> OverlayLayout {
    OverlayLayout layout;
    layout.canvas_width = proto.canvas_width();
    layout.canvas_height = proto.canvas_height();
    for (const auto& element : proto.elements()) {
        layout.elements.push_back(MapElement(element));
    }
    for (const auto& tmpl : proto.templates()) {
        OverlayTemplate domain_tmpl;
        domain_tmpl.event_type = tmpl.event_type();
        domain_tmpl.duration_ms = tmpl.duration_ms();
        for (const auto& element : tmpl.elements()) {
            domain_tmpl.elements.push_back(MapElement(element));
        }
        layout.templates.push_back(std::move(domain_tmpl));
    }
    return layout;
}

}  // namespace sst::overlay
