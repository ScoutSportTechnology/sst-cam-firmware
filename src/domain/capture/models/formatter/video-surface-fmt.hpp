#pragma once

#include <fmt/format.h>

#include "domain/capture/models/video-surface.hpp"
#include "domain/common/models/formatter/pixel-format-fmt.hpp"  // IWYU pragma: keep

template <>
struct fmt::formatter<sst::capture::VideoSurface> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::capture::VideoSurface& surface, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "VideoSurface{{camera={} pts={} {}x{} {} bytes={}}}",
                              surface.camera_index, surface.capture_pts.count(),
                              surface.geometry.width, surface.geometry.height, surface.format,
                              surface.bytes);
    }
};
