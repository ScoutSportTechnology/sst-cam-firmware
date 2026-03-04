#pragma once

#include <fmt/format.h>

#include "domain/config/models/profile.hpp"

template <>
struct fmt::formatter<sst::config::ProfileData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::ProfileData& data, FormatContext& ctx) const {
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
