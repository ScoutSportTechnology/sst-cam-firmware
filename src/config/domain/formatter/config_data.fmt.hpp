#pragma once

#include <fmt/format.h>

#include "config/domain/config_data.hpp"
#include "config/domain/formatter/calibration_config.fmt.hpp"
#include "config/domain/formatter/device_config.fmt.hpp"
#include "config/domain/formatter/storage_config.fmt.hpp"
#include "config/domain/formatter/stream_config.fmt.hpp"


template <>
struct fmt::formatter<sst::config::domain::ConfigData> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::ConfigData& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "ConfigData{{\n"
                              "  calibration={},\n"
                              "  device={},\n"
                              "  storage={},\n"
                              "  stream={}\n"
                              "}}",
                              data.calibration, data.device, data.storage, data.stream);
    }
};
