#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "domain/config/models/calibration.hpp"
#include "fmt-helper.hpp"

template <>
struct fmt::formatter<sst::config::domain::CalibrationCameraData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::CalibrationCameraData& data, FormatContext& ctx) const {
        using namespace sst::config::domain;

        return fmt::format_to(
            ctx.out(),
            "CalibrationCameraData{{\n"
            "  id={},\n"
            "  exposure={},\n"
            "  gain={},\n"
            "  white_balance={},\n"
            "  focus={},\n"
            "  width={},\n"
            "  height={},\n"
            "  format={},\n"
            "  fps={},\n"
            "  intrinsic_matrix={},\n"
            "  distortion_coefficients={},\n"
            "  last_calibration_date={}\n"
            "}}",
            NumOptToStr(data.id), NumOptToStr(data.exposure), NumOptToStr(data.gain),
            StrOptToStr(data.white_balance), StrOptToStr(data.focus), NumOptToStr(data.width),
            NumOptToStr(data.height), StrOptToStr(data.format), NumOptToStr(data.fps),
            ArrOptToStr(data.intrinsic_matrix), ArrOptToStr(data.distortion_coefficients),
            StrOptToStr(data.last_calibration_date));
    }
};

template <>
struct fmt::formatter<sst::config::domain::CalibrationMicrophoneData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::CalibrationMicrophoneData& data,
                FormatContext& ctx) const {
        using namespace sst::config::domain;

        return fmt::format_to(ctx.out(),
                              "CalibrationMicrophoneData{{\n"
                              "  last_calibration_date={},\n"
                              "  id={},\n"
                              "  sensitivity={},\n"
                              "  noise_reduction={}\n"
                              "}}",
                              StrOptToStr(data.last_calibration_date), NumOptToStr(data.id),
                              NumOptToStr(data.sensitivity), BoolOptToStr(data.noise_reduction));
    }
};

template <>
struct fmt::formatter<sst::config::domain::CalibrationDevicesData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::CalibrationDevicesData& data, FormatContext& ctx) const {
        using namespace sst::config::domain;

        return fmt::format_to(ctx.out(),
                              "CalibrationDevicesData{{\n"
                              "  camera={},\n"
                              "  microphone={}\n"
                              "}}",
                              VecOptToStr(data.camera), VecOptToStr(data.microphone));
    }
};

template <>
struct fmt::formatter<sst::config::domain::CalibrationData> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::config::domain::CalibrationData& data, FormatContext& ctx) const {
        using namespace sst::config::domain;

        const auto devices = data.devices ? fmt::format("{}", *data.devices) : "null";
        return fmt::format_to(ctx.out(),
                              "CalibrationData{{\n"
                              "  devices={}\n"
                              "}}",
                              devices);
    }
};
