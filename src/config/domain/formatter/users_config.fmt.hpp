#pragma once

#include <fmt/format.h>

#include "config/domain/users_config.hpp"

template <>
struct fmt::formatter<sst::config::domain::UsersData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::UsersData& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "UsersData{{\n"
                              "  name=\"{}\"\n"
                              "}}",
                              data.name);
    }
};