#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "domain/config/models/stream.hpp"

template <>
struct fmt::formatter<sst::config::StreamPlatformSettingsData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StreamPlatformSettingsData& data, FormatContext& ctx) const {
        const auto width = data.width ? fmt::format("{}", *data.width) : "null";
        const auto height = data.height ? fmt::format("{}", *data.height) : "null";
        const auto framerate = data.framerate ? fmt::format("{}", *data.framerate) : "null";
        const auto bitrate_kbps =
            data.bitrate_kbps ? fmt::format("{}", *data.bitrate_kbps) : "null";

        return fmt::format_to(ctx.out(),
                              "StreamPlatformSettingsData{{\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  framerate={},\n"
                              "  bitrate_kbps={}\n"
                              "}}",
                              width, height, framerate, bitrate_kbps);
    }
};

template <>
struct fmt::formatter<sst::config::StreamPlatformData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StreamPlatformData& data, FormatContext& ctx) const {
        std::string enabled = "null";
        if (data.enabled) {
            enabled = *data.enabled ? "true" : "false";
        }
        const auto url = data.url ? fmt::format("\"{}\"", *data.url) : "null";
        const auto key = data.key ? fmt::format("\"{}\"", *data.key) : "null";
        const auto stream_type =
            data.stream_type ? fmt::format("\"{}\"", *data.stream_type) : "null";
        const auto codec = data.codec ? fmt::format("\"{}\"", *data.codec) : "null";
        const auto settings = data.settings ? fmt::format("{}", *data.settings) : "null";

        return fmt::format_to(ctx.out(),
                              "StreamPlatformData{{\n"
                              "  enabled={},\n"
                              "  url={},\n"
                              "  key={},\n"
                              "  stream_type={},\n"
                              "  codec={},\n"
                              "  settings={}\n"
                              "}}",
                              enabled, url, key, stream_type, codec, settings);
    }
};

template <>
struct fmt::formatter<sst::config::StreamData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::StreamData& data, FormatContext& ctx) const {
        const auto youtube = data.youtube ? fmt::format("{}", *data.youtube) : "null";
        const auto twitch = data.twitch ? fmt::format("{}", *data.twitch) : "null";
        const auto facebook = data.facebook ? fmt::format("{}", *data.facebook) : "null";
        const auto instagram = data.instagram ? fmt::format("{}", *data.instagram) : "null";
        const auto tik_tok = data.tik_tok ? fmt::format("{}", *data.tik_tok) : "null";

        std::string custom = "null";
        if (data.custom) {
            custom = fmt::format("[{}]", fmt::join(*data.custom, ","));
        }

        return fmt::format_to(ctx.out(),
                              "StreamData{{\n"
                              "  youtube={},\n"
                              "  twitch={},\n"
                              "  facebook={},\n"
                              "  instagram={},\n"
                              "  tik_tok={},\n"
                              "  custom={}\n"
                              "}}",
                              youtube, twitch, facebook, instagram, tik_tok, custom);
    }
};
