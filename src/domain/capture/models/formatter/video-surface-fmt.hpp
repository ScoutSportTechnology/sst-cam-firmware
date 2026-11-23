#pragma once

#include <fmt/format.h>

#include "domain/capture/models/video-surface.hpp"

template <>
struct fmt::formatter<sst::capture::VideoSurface> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::capture::VideoSurface& surface, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "VideoSurface{{cam={} {}x{} pts={} bytes={}}}",
                              surface.camera_index, surface.geometry.width, surface.geometry.height,
                              surface.capture_pts.count(), surface.bytes);
    }
};
