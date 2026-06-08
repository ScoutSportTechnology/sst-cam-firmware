#pragma once

#include <fmt/format.h>

#include "domain/session/models/session-config.hpp"

template <>
struct fmt::formatter<sst::session::SessionConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::session::SessionConfig& c, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "SessionConfig{{match={}, user={}, sport={}, periods={}x{}s, "
                              "teams=[{}|{} vs {}|{}], video={}, thumb={}}}",
                              c.match_uuid, c.user_uuid, c.sport, c.num_periods,
                              c.period_length_seconds, c.team_a_name, c.team_a_id, c.team_b_name,
                              c.team_b_id, c.video_output_path, c.thumbnail_output_path);
    }
};
