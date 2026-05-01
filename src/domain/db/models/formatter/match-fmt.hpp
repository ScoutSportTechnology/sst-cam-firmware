#pragma once

#include <fmt/format.h>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/match.hpp"

template <>
struct fmt::formatter<sst::db::Match> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::Match& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "Match{{\n"
                              "  id=\"{}\",\n"
                              "  user_id={},\n"
                              "  sport_id={},\n"
                              "  team_a_id={},\n"
                              "  team_b_id={},\n"
                              "  name={},\n"
                              "  started_at=\"{}\",\n"
                              "  ended_at={},\n"
                              "  current_period={},\n"
                              "  final_score_a={},\n"
                              "  final_score_b={}\n"
                              "}}",
                              data.id, data.user_id, data.sport_id, data.team_a_id,
                              data.team_b_id, sst::db::StrOptToStr(data.name), data.started_at,
                              sst::db::StrOptToStr(data.ended_at), data.current_period,
                              data.final_score_a, data.final_score_b);
    }
};

template <>
struct fmt::formatter<sst::db::MatchEvent> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::MatchEvent& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "MatchEvent{{\n"
                              "  id=\"{}\",\n"
                              "  match_id=\"{}\",\n"
                              "  sport_id={},\n"
                              "  event_code=\"{}\",\n"
                              "  period={},\n"
                              "  timestamp_in_match={},\n"
                              "  wall_clock_at=\"{}\",\n"
                              "  metadata_json={}\n"
                              "}}",
                              data.id, data.match_id, data.sport_id, data.event_code, data.period,
                              data.timestamp_in_match, data.wall_clock_at,
                              sst::db::StrOptToStr(data.metadata_json));
    }
};
