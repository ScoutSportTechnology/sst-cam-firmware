#pragma once

#include <fmt/format.h>

#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

template <>
struct fmt::formatter<sst::pipeline::FramePair> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::pipeline::FramePair& pair, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "FramePair{{pts={} skew={} discontinuity={}}}",
                              pair.pts.count(), pair.skew.count(), pair.discontinuity);
    }
};
