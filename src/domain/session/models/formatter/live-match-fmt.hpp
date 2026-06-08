#pragma once

#include <fmt/format.h>

#include "domain/session/models/live-match.hpp"

template <>
struct fmt::formatter<sst::session::LiveMatch> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::session::LiveMatch& m, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "LiveMatch{{score={}-{}, period={}, clock={}s, running={}}}",
                              m.score_a, m.score_b, m.period, m.clock_seconds, m.clock_running);
    }
};
