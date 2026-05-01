#pragma once

#include <fmt/format.h>

#include "domain/db/models/sport.hpp"

template <>
struct fmt::formatter<sst::db::Sport> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::Sport& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "Sport{{\n"
                              "  id={},\n"
                              "  code=\"{}\",\n"
                              "  name=\"{}\",\n"
                              "  periods={}\n"
                              "}}",
                              data.id, data.code, data.name, data.periods);
    }
};

template <>
struct fmt::formatter<sst::db::SportEventKind> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::SportEventKind& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "SportEventKind{{\n"
                              "  sport_id={},\n"
                              "  code=\"{}\",\n"
                              "  name=\"{}\"\n"
                              "}}",
                              data.sport_id, data.code, data.name);
    }
};
