#pragma once

#include <fmt/format.h>

#include <cstdint>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/stream-config.hpp"

template <>
struct fmt::formatter<sst::db::StreamPlatform> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::StreamPlatform data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::StreamType> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::StreamType data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::StreamCodec> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::StreamCodec data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::StreamConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::StreamConfig& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "StreamConfig{{\n"
                              "  id={},\n"
                              "  user_id={},\n"
                              "  platform={},\n"
                              "  name=\"{}\",\n"
                              "  enabled={},\n"
                              "  stream_key={},\n"
                              "  stream_type={},\n"
                              "  url={},\n"
                              "  codec={},\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  framerate={},\n"
                              "  bitrate_kbps={}\n"
                              "}}",
                              data.id, data.user_id, data.platform, data.name,
                              sst::db::BoolToStr(data.enabled),
                              sst::db::StrOptToStr(data.stream_key), data.stream_type,
                              sst::db::StrOptToStr(data.url), data.codec, data.width, data.height,
                              data.framerate, data.bitrate_kbps);
    }
};
