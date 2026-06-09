#pragma once

#include "app/overlay/services/overlay_scene/overlay-scene.hpp"
#include "domain/overlay/models/overlay-enums.hpp"
#include "domain/session/models/session-state.hpp"

namespace sst::overlay {

// Derive overlay binding data from the authoritative session state: team names
// from the session config, scores/clock/period/segment from the live match.
inline auto BuildBindingData(const sst::session::SessionState& state) -> BindingData {
    BindingData data;
    if (state.config) {
        data.team_a_name = state.config->team_a_name;
        data.team_b_name = state.config->team_b_name;
    }
    data.score_a = state.match.score_a;
    data.score_b = state.match.score_b;
    data.period = state.match.period;
    data.clock_seconds = state.match.clock_seconds;
    switch (state.match.segment) {
        case sst::session::MatchSegment::kHalfTime:
            data.period_state = PeriodLabelState::kHalfTime;
            break;
        case sst::session::MatchSegment::kFullTime:
            data.period_state = PeriodLabelState::kFullTime;
            break;
        case sst::session::MatchSegment::kInPlay:
            data.period_state = PeriodLabelState::kInPlay;
            break;
    }
    return data;
}

}  // namespace sst::overlay
