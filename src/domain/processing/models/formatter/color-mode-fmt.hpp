#pragma once

#include <fmt/format.h>

#include "domain/processing/models/color-mode.hpp"

template <>
struct fmt::formatter<sst::processing::ColorMode> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::processing::ColorMode mode, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", sst::processing::ToString(mode));
    }
};
