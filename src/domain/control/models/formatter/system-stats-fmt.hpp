#pragma once

#include <fmt/format.h>

#include "domain/control/models/system-stats.hpp"

template <>
struct fmt::formatter<sst::control::SystemStats> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::control::SystemStats& s, FormatContext& ctx) const {
        return fmt::format_to(
            ctx.out(),
            "SystemStats{{storage={}/{} bytes, temp={:.1f}C, ram={:.1f}%, cpu={:.1f}%, "
            "uptime={}s, battery={}%}}",
            s.storage_free_bytes, s.storage_total_bytes, s.temp_celsius, s.ram_used_pct,
            s.cpu_used_pct, s.uptime_seconds, s.battery_level_pct);
    }
};
