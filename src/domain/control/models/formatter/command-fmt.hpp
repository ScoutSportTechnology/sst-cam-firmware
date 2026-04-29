#pragma once

#include <fmt/format.h>

#include "domain/control/models/command.hpp"

template <>
struct fmt::formatter<sst::control::Command> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::control::Command& cmd, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "Command{{\n"
                              "  route=\"{}\",\n"
                              "  payload_size={},\n"
                              "  correlation_id={}\n"
                              "}}",
                              cmd.route, cmd.payload.size(), cmd.correlation_id);
    }
};
