#pragma once

#include <fmt/format.h>

#include "domain/db/models/formatter/fmt-helper.hpp"
#include "domain/db/models/microphone.hpp"

template <>
struct fmt::formatter<sst::db::MicrophoneConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::MicrophoneConfig& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "MicrophoneConfig{{\n"
                              "  user_id={},\n"
                              "  noise_reduction={}\n"
                              "}}",
                              data.user_id, sst::db::BoolToStr(data.noise_reduction));
    }
};

template <>
struct fmt::formatter<sst::db::MicrophoneCalibration> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::MicrophoneCalibration& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "MicrophoneCalibration{{\n"
                              "  id={},\n"
                              "  mic_id={},\n"
                              "  sensitivity={},\n"
                              "  calibrated_at=\"{}\"\n"
                              "}}",
                              data.id, data.mic_id, data.sensitivity, data.calibrated_at);
    }
};
