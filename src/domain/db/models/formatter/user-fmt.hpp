#pragma once

#include <fmt/format.h>

#include "domain/db/models/user.hpp"

template <>
struct fmt::formatter<sst::db::User> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::User& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "User{{\n"
                              "  id={},\n"
                              "  username=\"{}\"\n"
                              "}}",
                              data.id, data.username);
    }
};
