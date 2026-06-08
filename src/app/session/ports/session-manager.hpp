#pragma once

#include <functional>

#include "domain/session/models/live-match.hpp"
#include "domain/session/models/session-config.hpp"
#include "domain/session/models/session-phase.hpp"
#include "domain/session/models/session-state.hpp"

namespace sst::session {

// Drives the ordered session lifecycle (F1–F3) and owns the in-memory session
// state. Transitions advance only in order and reject out-of-order requests;
// the only backward edge is OnDisconnect → Idle, which also fans out cleanup
// and clears all session memory. Implementations are thread-safe: the BLE
// thread and event handlers call in concurrently.
class ISessionManager {
   public:
    virtual ~ISessionManager() = default;

    [[nodiscard]] virtual auto Phase() const -> SessionPhase = 0;
    [[nodiscard]] virtual auto Snapshot() const -> SessionState = 0;

    // Lifecycle transitions. Each returns true if the transition was allowed in
    // the current phase, false (and a no-op) otherwise.
    virtual auto OnConnect() -> bool = 0;
    virtual auto OnWifiReady() -> bool = 0;
    virtual auto OnWifiStopped() -> bool = 0;
    virtual auto ApplySessionConfig(const SessionConfig& config) -> bool = 0;
    virtual auto OnOverlayConfigured() -> bool = 0;
    virtual auto OnRecordingStart() -> bool = 0;
    virtual auto OnRecordingStop() -> bool = 0;

    // Session end: fan out cleanup (finalize recording, close RTMP, tear down
    // WiFi Direct) and clear all session memory back to Idle.
    virtual auto OnDisconnect() -> void = 0;

    // Apply a display-only live-match mutation (score/clock/period) while a
    // session is active (phase >= Configured). Returns false (no-op) if there
    // is no active session.
    virtual auto ApplyMatchUpdate(const std::function<void(LiveMatch&)>& mutate) -> bool = 0;
};

}  // namespace sst::session
