#pragma once

#include <fmt/format.h>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/team.hpp"

template <>
struct fmt::formatter<sst::db::Team> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::Team& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "Team{{\n"
                              "  id={},\n"
                              "  sport_id={},\n"
                              "  name=\"{}\",\n"
                              "  short_name=\"{}\"\n"
                              "}}",
                              data.id, data.sport_id, data.name, data.short_name);
    }
};

template <>
struct fmt::formatter<sst::db::Player> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::Player& data, FormatContext& ctx) const {
        return fmt::format_to(
            ctx.out(),
            "Player{{\n"
            "  id={},\n"
            "  team_id={},\n"
            "  name=\"{}\",\n"
            "  jersey_number={}\n"
            "}}",
            data.id, data.team_id, data.name,
            data.jersey_number.has_value() ? std::to_string(*data.jersey_number) : "null");
    }
};
