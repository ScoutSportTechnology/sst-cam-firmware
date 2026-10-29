#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/session/models/live-match.hpp"
#include "domain/session/models/session-config.hpp"
#include "domain/session/models/session-phase.hpp"

namespace sst::session {

enum class SessionEndReason : std::uint8_t { kAppStop, kAutoStop, kCameraFailure, kReboot };

struct LastSession {
    std::string match_uuid;
    SessionEndReason reason{SessionEndReason::kAppStop};
    std::uint32_t clock_seconds{0};
    bool file_valid{false};
};

struct SessionState {
    SessionPhase phase{SessionPhase::kIdle};
    std::optional<SessionConfig> config;
    LiveMatch match;
    std::optional<LastSession> last_session;
    bool wifi_group_up{false};
    bool is_streaming{false};
    std::optional<std::uint32_t> recording_elapsed_seconds;
};

}  // namespace sst::session
