#pragma once

#include <fmt/format.h>

#include "domain/streaming/models/app-stream-config.hpp"

template <>
struct fmt::formatter<sst::streaming::AppStreamConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::streaming::AppStreamConfig& c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "AppStreamConfig{{\n"
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
