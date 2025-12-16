#pragma once

#include <fmt/format.h>

#include "config/domain/profile_config.hpp"

template <>
struct fmt::formatter<sst::config::domain::ProfileData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::ProfileData& data, FormatContext& ctx) const {
        const char* calibration = "null";
        if (data.calibration) {
            calibration = *data.calibration ? "true" : "false";
        }
        const char* device = "null";
        if (data.device) {
            device = *data.device ? "true" : "false";
        }
        const char* stream = "null";
        if (data.stream) {
            stream = *data.stream ? "true" : "false";
        }
        const char* storage = "null";
        if (data.storage) {
            storage = *data.storage ? "true" : "false";
        }

        return fmt::format_to(ctx.out(),
                              "ProfileData{{\n"
                              "  calibration={},\n"
                              "  device={},\n"
                              "  stream={},\n"
                              "  storage={}\n"
                              "}}",
                              calibration, device, stream, storage);
    }
};
