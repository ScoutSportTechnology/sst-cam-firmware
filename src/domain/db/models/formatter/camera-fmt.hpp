#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cstdint>

#include "domain/common/models/formatter/pixel-format-fmt.hpp"  // IWYU pragma: keep
#include "domain/db/models/camera.hpp"

template <>
struct fmt::formatter<sst::db::CameraWhiteBalance> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::CameraWhiteBalance data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::CameraFocus> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::db::CameraFocus data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint8_t>(data));
    }
};

template <>
struct fmt::formatter<sst::db::CameraConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::CameraConfig& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "CameraConfig{{\n"
                              "  user_id={},\n"
                              "  exposure={},\n"
                              "  gain={},\n"
                              "  white_balance={},\n"
                              "  focus={},\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  format={},\n"
                              "  fps={},\n"
                              "  event_clip_pre_seconds={},\n"
                              "  event_clip_post_seconds={}\n"
                              "}}",
                              data.user_id, data.exposure, data.gain, data.white_balance,
                              data.focus, data.width, data.height, data.format, data.fps,
                              data.event_clip_pre_seconds, data.event_clip_post_seconds);
    }
};

template <>
struct fmt::formatter<sst::db::CameraCalibration> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::db::CameraCalibration& data, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "CameraCalibration{{\n"
                              "  id={},\n"
                              "  camera_id={},\n"
                              "  intrinsic_matrix=[{}],\n"
                              "  distortion_coefficients=[{}],\n"
                              "  calibrated_at=\"{}\"\n"
                              "}}",
                              data.id, data.camera_id, fmt::join(data.intrinsic_matrix, ","),
                              fmt::join(data.distortion_coefficients, ","), data.calibrated_at);
    }
};
