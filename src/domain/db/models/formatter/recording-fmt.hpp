#pragma once

#include <cstdint>

#include <fmt/format.h>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/recording.hpp"

template <>
struct fmt::formatter<sst::db::RecordingKind> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::RecordingKind data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::Recording> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::Recording& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "Recording{{\n"
                              "  id=\"{}\",\n"
                              "  match_id={},\n"
                              "  kind={},\n"
                              "  file_path=\"{}\",\n"
                              "  started_at=\"{}\",\n"
                              "  ended_at={}\n"
                              "}}",
                              data.id, sst::db::StrOptToStr(data.match_id), data.kind,
                              data.file_path, data.started_at,
                              sst::db::StrOptToStr(data.ended_at));
    }
};

template <>
struct fmt::formatter<sst::db::RecordingSegment> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::RecordingSegment& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "RecordingSegment{{\n"
                              "  id={},\n"
                              "  recording_id=\"{}\",\n"
                              "  sequence={},\n"
                              "  file_path=\"{}\",\n"
                              "  started_at=\"{}\",\n"
                              "  ended_at={}\n"
                              "}}",
                              data.id, data.recording_id, data.sequence, data.file_path,
                              data.started_at, sst::db::StrOptToStr(data.ended_at));
    }
};

template <>
struct fmt::formatter<sst::db::EventClip> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::EventClip& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "EventClip{{\n"
                              "  id=\"{}\",\n"
                              "  match_event_id=\"{}\",\n"
                              "  recording_id=\"{}\",\n"
                              "  file_path=\"{}\",\n"
                              "  pre_seconds={},\n"
                              "  post_seconds={}\n"
                              "}}",
                              data.id, data.match_event_id, data.recording_id, data.file_path,
                              data.pre_seconds, data.post_seconds);
    }
};
