#pragma once

#include <fmt/format.h>

#include "domain/buffer/models/buffer-policy.hpp"

template <>
struct fmt::formatter<sst::buffer::BufferPolicy> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const sst::buffer::BufferPolicy& policy, FormatContext& ctx) const {
        const char* name = "Unknown";
        switch (policy) {
            case sst::buffer::BufferPolicy::LatestOnly:
                name = "LatestOnly";
                break;
            case sst::buffer::BufferPolicy::DropOldest:
                name = "DropOldest";
                break;
        }
        return fmt::format_to(ctx.out(), "{}", name);
    }
};
