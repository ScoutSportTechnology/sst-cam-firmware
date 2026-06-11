#pragma once

#include <fmt/format.h>

#include "buffer-policy-fmt.hpp"  // IWYU pragma: keep
#include "domain/buffer/models/buffer-stats.hpp"

template <>
struct fmt::formatter<sst::buffer::BufferStats> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::buffer::BufferStats& stats, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "BufferStats{{\n"
                              "  policy={},\n"
                              "  capacity={},\n"
                              "  depth={},\n"
                              "  pushed={},\n"
                              "  popped={},\n"
                              "  dropped={}\n"
                              "}}",
                              stats.policy, stats.capacity, stats.depth, stats.pushed, stats.popped,
                              stats.dropped);
    }
};
