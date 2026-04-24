#pragma once

#include <fmt/format.h>

#include "calibration-fmt.hpp"  // IWYU pragma: keep
#include "device-fmt.hpp"       // IWYU pragma: keep
#include "domain/config/models/config-data.hpp"

template <>
struct fmt::formatter<sst::config::ConfigData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::ConfigData& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "ConfigData{{\n"
                              "  calibration={},\n"
                              "  device={}\n"
                              "}}",
                              data.calibration, data.device);
    }
};
