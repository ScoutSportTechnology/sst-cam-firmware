#pragma once

#include <fmt/format.h>

#include "domain/streaming/models/preview-config.hpp"

template <>
struct fmt::formatter<sst::streaming::PreviewConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::streaming::PreviewConfig& c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "PreviewConfig{{\n"
                              "  mount_point=\"{}\",\n"
                              "  port={},\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  framerate={},\n"
                              "  bitrate_kbps={}\n"
                              "}}",
                              c.mount_point, c.port, c.width, c.height, c.framerate,
                              c.bitrate_kbps);
    }
};
