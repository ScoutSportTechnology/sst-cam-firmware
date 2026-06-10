#pragma once

#include <fmt/format.h>

#include "domain/storage/models/raw-capture-identity.hpp"

template <>
struct fmt::formatter<sst::storage::RawCaptureIdentity> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::storage::RawCaptureIdentity& id, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "RawCaptureIdentity{{group={}, camera_index={}}}",
                              id.capture_group_id, id.camera_index);
    }
};
