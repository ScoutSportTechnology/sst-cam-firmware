#pragma once

#include <fmt/format.h>

#include "domain/common/models/pixel-format.hpp"

template <>
struct fmt::formatter<sst::common::PixelFormat> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::common::PixelFormat data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", sst::common::ToString(data));
    }
};
