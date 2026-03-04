#pragma once

#include <fmt/format.h>

#include "domain/config/models/users.hpp"

template <>
struct fmt::formatter<sst::config::UsersData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::UsersData& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "UsersData{{\n"
                              "  name=\"{}\"\n"
                              "}}",
                              data.name);
    }
};