#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

#include "app/session/ports/session-cleanup.hpp"
#include "app/session/ports/session-manager.hpp"
#include "domain/session/models/session-state.hpp"

namespace sst::session {

class SessionManager final : public ISessionManager {
   public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    explicit SessionManager(ISessionCleanup& cleanup, Clock now = std::chrono::steady_clock::now,
                            std::filesystem::path video_root = {});
    ~SessionManager() override;

    [[nodiscard]] auto Phase() const -> SessionPhase override;
    [[nodiscard]] auto Snapshot() const -> SessionState override;

    auto OnConnect() -> bool override;
    auto OnWifiReady() -> bool override;
    auto OnWifiStopped() -> bool override;
    auto ApplySessionConfig(const SessionConfig& config) -> bool override;
    auto OnOverlayConfigured() -> bool override;
    auto OnRecordingStart() -> bool override;
    auto OnRecordingStopRequested() -> RecordingStopResult override;
    auto WaitForFinalization() -> void override;
    auto OnDisconnect() -> void override;
    auto Tick() -> void override;
    auto EndSession(SessionEndReason reason) -> bool override;
    auto OnStreamingChanged(bool active) -> bool override;

    auto ApplyMatchUpdate(const std::function<void(LiveMatch&)>& mutate) -> bool override;
    auto ReconcileMatch(const std::function<void(LiveMatch&)>& mutate) -> bool override;

   private:
    // Creates the parent directories of the session's video + thumbnail output
    // paths. Returns false if either mkdir fails.
    static auto PrepareOutputDirs(const SessionConfig& config) -> bool;

    ISessionCleanup& cleanup_;
    Clock now_;
    std::filesystem::path video_root_;
    std::optional<std::chrono::steady_clock::time_point> disconnected_at_;
    std::optional<std::chrono::steady_clock::time_point> recording_started_at_;
    std::optional<std::chrono::steady_clock::time_point> match_clock_started_at_;
    auto AdvanceMatchClock() -> void;
    mutable std::mutex mtx_;
    SessionState state_;
    bool recording_only_finalizing_{false};
    bool finalization_committing_{false};
    std::mutex worker_mtx_;
    std::thread finalize_worker_;
    auto BeginFinalization(SessionEndReason reason, bool recording_only) -> bool;
    auto FinishSession(SessionState ending, SessionEndReason reason, bool recording_only) -> void;
    auto PersistStopSummary(const LastSession& summary) -> bool;
};

}  // namespace sst::session
