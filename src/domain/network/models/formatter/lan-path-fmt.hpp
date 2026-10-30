#pragma once

#include <fmt/format.h>

#include "domain/network/models/connectivity-mode.hpp"

template <>
struct fmt::formatter<sst::network::LanPath> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::network::LanPath path, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", sst::network::ToString(path));
    }
};
