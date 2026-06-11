#pragma once

#include <fmt/format.h>

#include <cstdint>

#include "domain/streaming/models/platform-stream-config.hpp"

template <>
struct fmt::formatter<sst::streaming::PlatformStreamType> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::streaming::PlatformStreamType t, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(t));
    }
};

template <>
struct fmt::formatter<sst::streaming::PlatformStreamCodec> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::streaming::PlatformStreamCodec c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(c));
    }
};

template <>
struct fmt::formatter<sst::streaming::PlatformStreamConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::streaming::PlatformStreamConfig& c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "PlatformStreamConfig{{\n"
                              "  stream_id={},\n"
                              "  name=\"{}\",\n"
                              "  type={},\n"
                              "  url=\"{}\",\n"
                              "  stream_key=\"{}\",\n"
                              "  codec={},\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  framerate={},\n"
                              "  bitrate_kbps={}\n"
                              "}}",
                              c.stream_id, c.name, c.type, c.url, c.stream_key, c.codec, c.width,
                              c.height, c.framerate, c.bitrate_kbps);
    }
};
