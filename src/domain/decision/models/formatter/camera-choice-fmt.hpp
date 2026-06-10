#pragma once

#include <fmt/format.h>

#include "domain/decision/models/camera-choice.hpp"
#include "domain/processing/models/formatter/crop-rect-fmt.hpp"  // IWYU pragma: keep

template <>
struct fmt::formatter<sst::decision::CameraChoice> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::decision::CameraChoice& c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "CameraChoice{{camera_index={}, crop={}}}", c.camera_index,
                              c.crop);
    }
};
