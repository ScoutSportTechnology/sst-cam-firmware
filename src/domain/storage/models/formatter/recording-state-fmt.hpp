#pragma once

#include <fmt/format.h>

#include <cstdint>

#include "domain/storage/models/recording-state.hpp"

template <>
struct fmt::formatter<sst::storage::RecordingState> {
    static constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(sst::storage::RecordingState state, FormatContext& ctx) const {
        const char* name = "?";
        switch (state) {
            case sst::storage::RecordingState::kIdle:
                name = "Idle";
                break;
            case sst::storage::RecordingState::kRecording:
                name = "Recording";
                break;
            case sst::storage::RecordingState::kPaused:
                name = "Paused";
                break;
        }
        return fmt::format_to(ctx.out(), "{}", name);
    }
};
