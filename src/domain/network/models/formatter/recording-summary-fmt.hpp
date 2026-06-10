#pragma once

#include <fmt/format.h>

#include "domain/network/models/recording-summary.hpp"

template <>
struct fmt::formatter<sst::network::RecordingSummary> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::network::RecordingSummary& r, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "RecordingSummary{{id={}, size={} bytes, started_at={}, "
                              "duration={}s, thumbnail={}, is_raw={}, camera_index={}, group={}}}",
                              r.recording_id, r.size_bytes, r.started_at_unix, r.duration_s,
                              r.thumbnail_id, r.is_raw, r.camera_index, r.capture_group_id);
    }
};
