#pragma once

#include <optional>

#include "domain/session/models/live-match.hpp"
#include "domain/session/models/session-config.hpp"
#include "domain/session/models/session-phase.hpp"

namespace sst::session {

// The complete in-memory session. Lives only between BLE connect and
// disconnect; cleared on session end (R11). `config` is set once
// PushSessionConfig arrives; `match` accumulates display-only score/clock.
struct SessionState {
    SessionPhase phase{SessionPhase::kIdle};
    std::optional<SessionConfig> config;
    LiveMatch match;
};

}  // namespace sst::session
