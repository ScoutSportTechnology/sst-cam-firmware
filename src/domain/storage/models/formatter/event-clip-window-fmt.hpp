#pragma once

#include <fmt/format.h>

#include "domain/storage/models/event-clip-window.hpp"

template <>
struct fmt::formatter<sst::storage::EventClipWindow> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::storage::EventClipWindow& w, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "EventClipWindow{{pre={}, post={}}}", w.pre_seconds,
                              w.post_seconds);
    }
};
