#pragma once

#include <functional>
#include <mutex>

#include "app/session/ports/session-cleanup.hpp"
#include "app/session/ports/session-manager.hpp"
#include "domain/session/models/session-state.hpp"

namespace sst::session {

// Concrete session lifecycle state machine. Holds the in-memory SessionState
// and enforces the ordered transitions; ApplySessionConfig prepares the output
// directories; OnDisconnect fans out cleanup and clears memory.
class SessionManager final : public ISessionManager {
   public:
    explicit SessionManager(ISessionCleanup& cleanup);

    [[nodiscard]] auto Phase() const -> SessionPhase override;
    [[nodiscard]] auto Snapshot() const -> SessionState override;

    auto OnConnect() -> bool override;
    auto OnWifiReady() -> bool override;
    auto OnWifiStopped() -> bool override;
    auto ApplySessionConfig(const SessionConfig& config) -> bool override;
    auto OnOverlayConfigured() -> bool override;
    auto OnRecordingStart() -> bool override;
    auto OnRecordingStop() -> bool override;
    auto OnDisconnect() -> void override;

    auto ApplyMatchUpdate(const std::function<void(LiveMatch&)>& mutate) -> bool override;

   private:
    // Creates the parent directories of the session's video + thumbnail output
    // paths. Returns false if either mkdir fails.
    static auto PrepareOutputDirs(const SessionConfig& config) -> bool;

    ISessionCleanup& cleanup_;
    mutable std::mutex mtx_;
    SessionState state_;
};

}  // namespace sst::session
