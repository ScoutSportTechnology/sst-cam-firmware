#pragma once

#include <fmt/format.h>

#include "domain/streaming/models/app-stream-config.hpp"

template <>
struct fmt::formatter<sst::streaming::AppStreamConfig> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::streaming::AppStreamConfig& cfg, FormatContext& ctx) const {
        // auth_credential is a secret — log only whether one is set, never its value.
        return fmt::format_to(ctx.out(),
                              "AppStreamConfig{{\n"
                              "  address=\"{}\",\n"
                              "  mount_point=\"{}\",\n"
                              "  port={},\n"
                              "  width={},\n"
                              "  height={},\n"
                              "  framerate={},\n"
                              "  bitrate_kbps={},\n"
                              "  auth={}\n"
                              "}}",
                              cfg.address, cfg.mount_point, cfg.port, cfg.width, cfg.height,
                              cfg.framerate, cfg.bitrate_kbps,
                              cfg.auth_credential.empty() ? "none" : "required");
    }
};
