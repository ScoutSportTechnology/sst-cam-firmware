#pragma once

#include <fmt/format.h>

#include "domain/control/models/command-result.hpp"

template <>
struct fmt::formatter<sst::control::CommandStatus> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::control::CommandStatus s, FormatContext& ctx) const {
        using sst::control::CommandStatus;
        const char* name = "Unknown";
        switch (s) {
            case CommandStatus::kOk: name = "Ok"; break;
            case CommandStatus::kInvalidArgument: name = "InvalidArgument"; break;
            case CommandStatus::kNotFound: name = "NotFound"; break;
            case CommandStatus::kFailedPrecondition: name = "FailedPrecondition"; break;
            case CommandStatus::kUnimplemented: name = "Unimplemented"; break;
            case CommandStatus::kInternal: name = "Internal"; break;
        }
        return fmt::format_to(ctx.out(), "{}", name);
    }
};

template <>
struct fmt::formatter<sst::control::CommandResult> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::control::CommandResult& r, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "CommandResult{{\n"
                              "  status={},\n"
                              "  payload_size={},\n"
                              "  correlation_id={}\n"
                              "}}",
                              r.status, r.payload.size(), r.correlation_id);
    }
};
