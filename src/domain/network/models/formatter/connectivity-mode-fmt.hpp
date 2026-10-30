#pragma once

#include <fmt/format.h>

#include "domain/network/models/connectivity-mode.hpp"

template <>
struct fmt::formatter<sst::network::ConnectivityMode> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::network::ConnectivityMode mode, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", sst::network::ToString(mode));
    }
};
