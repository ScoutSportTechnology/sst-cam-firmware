#include "app/session/services/session_manager/session-manager.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#include "domain/session/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::session {

namespace fs = std::filesystem;

namespace {
auto ActiveMarkerPath(const std::string& output) -> fs::path {
    fs::path path(output);
    if (output.back() == '/' || !path.has_extension()) {
        const auto name = path.has_filename() ? path.filename() : path.parent_path().filename();
        path /= name;
    }
    return path.replace_extension(".session-active");
}
}  // namespace

SessionManager::SessionManager(ISessionCleanup& cleanup, Clock now, fs::path video_root)
    : cleanup_(cleanup), now_(std::move(now)), video_root_(std::move(video_root)) {
    if (video_root_.empty()) {
        return;
    }
    const fs::path summary_path = video_root_ / ".last-session-shutdown";
    std::ifstream file(summary_path);
    std::string match_uuid;
    std::string clock_text;
    std::string reason_text;
    std::error_code error;
    const auto summary_time = fs::last_write_time(summary_path, error);
    if (!error && std::getline(file, match_uuid) && std::getline(file, clock_text) &&
        !match_uuid.empty() && match_uuid.find('/') == std::string::npos) {
        std::uint32_t seconds = 0;
        const auto [end, parse_error] =
            std::from_chars(clock_text.data(), clock_text.data() + clock_text.size(), seconds);
        const bool has_reason = static_cast<bool>(std::getline(file, reason_text));
        if (parse_error == std::errc{} && end == clock_text.data() + clock_text.size() &&
            (!has_reason || reason_text == "app-stop") &&
            file.peek() == std::char_traits<char>::eof()) {
            state_.last_session =
                LastSession{match_uuid,
                            reason_text == "app-stop" ? SessionEndReason::kAppStop
                                                      : SessionEndReason::kShutdown,
                            seconds, true};
        }
    }
    const bool has_summary = state_.last_session.has_value();
    error.clear();
    fs::file_time_type newest_orphan{};
    bool has_orphan = false;
    for (fs::recursive_directory_iterator entry(video_root_, error), end; !error && entry != end;
         entry.increment(error)) {
        if (entry->is_regular_file(error) && entry->path().extension() == ".session-active") {
            const auto marker = entry->path();
            const auto marker_time = fs::last_write_time(marker, error);
            if (!error && (!has_summary || marker_time >= summary_time) &&
                (!has_orphan || marker_time >= newest_orphan)) {
                state_.last_session = LastSession{marker.parent_path().filename().string(),
                                                  SessionEndReason::kReboot, 0, false};
                newest_orphan = marker_time;
                has_orphan = true;
            }
            error.clear();
            fs::remove(marker, error);
        }
    }
}

SessionManager::~SessionManager() { WaitForFinalization(); }

auto SessionManager::WaitForFinalization() -> void {
    std::lock_guard lock(worker_mtx_);
    if (finalize_worker_.joinable()) {
        finalize_worker_.join();
    }
}

auto SessionManager::Phase() const -> SessionPhase {
    std::lock_guard lock(mtx_);
    return state_.phase;
}

auto SessionManager::Snapshot() const -> SessionState {
    std::lock_guard lock(mtx_);
    auto snapshot = state_;
    if (match_clock_started_at_) {
        const auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now_() - *match_clock_started_at_)
                .count();
        if (seconds > 0) {
            snapshot.match.clock_seconds += static_cast<std::uint32_t>(seconds);
        }
    }
    if (recording_started_at_ && state_.phase == SessionPhase::kRecording) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now_() - *recording_started_at_)
                .count();
        snapshot.recording_elapsed_seconds = static_cast<std::uint32_t>(
            std::clamp<std::int64_t>(elapsed, 0, std::numeric_limits<std::uint32_t>::max()));
    }
    return snapshot;
}

auto SessionManager::OnConnect() -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase == SessionPhase::kFinalizing) {
        return false;
    }
    disconnected_at_.reset();
    if (state_.phase != SessionPhase::kIdle) {
        return true;
    }
    state_.phase = SessionPhase::kConnected;
    spdlog::info("SessionManager: -> Connected");
    return true;
}

auto SessionManager::OnWifiReady() -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase < SessionPhase::kConnected || state_.phase == SessionPhase::kFinalizing) {
        spdlog::warn("SessionManager::OnWifiReady rejected: phase={}", state_.phase);
        return false;
    }
    // Only advance from Connected; re-affirming when already further along is a
    // no-op success (the group is already up).
    if (state_.phase == SessionPhase::kConnected) {
        state_.phase = SessionPhase::kWifiReady;
        spdlog::info("SessionManager: -> WifiReady");
    }
    state_.wifi_group_up = true;
    return true;
}

auto SessionManager::OnWifiStopped() -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase == SessionPhase::kIdle || state_.phase == SessionPhase::kFinalizing ||
        state_.phase == SessionPhase::kRecording || state_.is_streaming) {
        return false;
    }
    // Tearing down the group invalidates everything built on top of it; drop
    // back to Connected and discard the session config.
    state_.phase = SessionPhase::kConnected;
    state_.wifi_group_up = false;
    state_.config.reset();
    state_.match = LiveMatch{};
    match_clock_started_at_.reset();
    disconnected_at_.reset();
    spdlog::info("SessionManager: WiFi stopped -> Connected (session config cleared)");
    return true;
}

auto SessionManager::ApplySessionConfig(const SessionConfig& config) -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase < SessionPhase::kWifiReady || state_.phase == SessionPhase::kFinalizing ||
        ((state_.phase == SessionPhase::kRecording || state_.is_streaming) &&
         (!state_.config || state_.config->match_uuid != config.match_uuid))) {
        spdlog::warn(
            "SessionManager::ApplySessionConfig rejected (out of order: WiFi Direct group "
            "required first per contract §11): phase={}",
            state_.phase);
        return false;
    }
    if (state_.config && state_.config->match_uuid == config.match_uuid &&
        (state_.phase == SessionPhase::kRecording || state_.is_streaming)) {
        state_.config->auto_stop_minutes = config.auto_stop_minutes;
        if (disconnected_at_) {
            disconnected_at_ = now_();
        }
        return true;
    }
    if (!PrepareOutputDirs(config)) {
        return false;
    }
    const bool is_new_match =
        !state_.config.has_value() || state_.config->match_uuid != config.match_uuid;
    if (is_new_match && !video_root_.empty()) {
        std::error_code error;
        fs::remove(video_root_ / ".last-session-shutdown", error);
        if (error) {
            spdlog::error("SessionManager: failed to clear shutdown summary: {}", error.message());
            return false;
        }
    }
    state_.config = config;
    if (is_new_match) {
        state_.last_session.reset();
    }
    if (disconnected_at_) {
        disconnected_at_ = now_();
    }
    if (is_new_match) {
        state_.match = LiveMatch{};
        match_clock_started_at_.reset();
    }
    // First config advances WifiReady -> Configured; a re-push while further
    // along keeps the current phase.
    if (state_.phase == SessionPhase::kWifiReady) {
        state_.phase = SessionPhase::kConfigured;
    }
    spdlog::info("SessionManager: session config applied {} (phase={})", config, state_.phase);
    return true;
}

auto SessionManager::OnOverlayConfigured() -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase < SessionPhase::kConfigured || state_.phase == SessionPhase::kFinalizing) {
        spdlog::warn(
            "SessionManager::OnOverlayConfigured rejected: phase={} (need a session "
            "config first)",
            state_.phase);
        return false;
    }
    if (state_.phase == SessionPhase::kConfigured) {
        state_.phase = SessionPhase::kReady;
        spdlog::info("SessionManager: -> Ready");
    }
    // Re-push of the layout while Ready/Recording is allowed and keeps phase.
    return true;
}

auto SessionManager::OnRecordingStart() -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase != SessionPhase::kReady) {
        spdlog::warn("SessionManager::OnRecordingStart rejected: phase={}", state_.phase);
        return false;
    }
    state_.phase = SessionPhase::kRecording;
    recording_started_at_ = now_();
    if (!video_root_.empty() && state_.config && !state_.config->video_output_path.empty()) {
        std::ofstream marker(ActiveMarkerPath(state_.config->video_output_path));
        if (!marker) {
            spdlog::error("SessionManager: failed to write active-session marker");
        }
    }
    spdlog::info("SessionManager: -> Recording");
    return true;
}

auto SessionManager::OnRecordingStopRequested() -> RecordingStopResult {
    if (BeginFinalization(SessionEndReason::kAppStop, true)) {
        return RecordingStopResult::kStarted;
    }
    return Phase() == SessionPhase::kFinalizing ? RecordingStopResult::kAlreadyStopping
                                                : RecordingStopResult::kIdle;
}

auto SessionManager::OnDisconnect() -> void {
    std::lock_guard lock(mtx_);
    if (state_.phase != SessionPhase::kIdle && state_.phase != SessionPhase::kFinalizing &&
        !disconnected_at_) {
        disconnected_at_ = now_();
    }
}

auto SessionManager::Tick() -> void {
    bool expired = false;
    {
        std::lock_guard lock(mtx_);
        if (disconnected_at_ && state_.phase != SessionPhase::kFinalizing) {
            const auto minutes =
                state_.config ? state_.config->auto_stop_minutes : kDefaultAutoStopMinutes;
            expired = now_() - *disconnected_at_ >= std::chrono::minutes(minutes);
        }
    }
    if (expired) {
        EndSession(SessionEndReason::kAutoStop);
    }
}

auto SessionManager::OnStreamingChanged(bool active) -> bool {
    bool end = false;
    {
        std::lock_guard lock(mtx_);
        if (state_.phase == SessionPhase::kIdle ||
            (state_.phase == SessionPhase::kFinalizing &&
             (active || !recording_only_finalizing_ || finalization_committing_))) {
            return false;
        }
        if (active && !state_.config) {
            return false;
        }
        state_.is_streaming = active;
        end = !active && state_.phase == SessionPhase::kReady;
    }
    if (end) {
        EndSession(SessionEndReason::kAppStop);
    }
    return true;
}

auto SessionManager::EndSession(SessionEndReason reason) -> bool {
    if (reason == SessionEndReason::kShutdown || reason == SessionEndReason::kCameraFailure) {
        bool recording_only = false;
        {
            std::lock_guard lock(mtx_);
            recording_only =
                state_.phase == SessionPhase::kFinalizing && recording_only_finalizing_;
        }
        if (recording_only) {
            WaitForFinalization();
        }
    }
    if (reason == SessionEndReason::kShutdown) {
        std::lock_guard lock(mtx_);
        if (state_.phase == SessionPhase::kIdle && state_.last_session &&
            state_.last_session->reason == SessionEndReason::kAppStop &&
            state_.last_session->file_valid) {
            (void)PersistStopSummary(*state_.last_session);
        }
    }
    return BeginFinalization(reason, false);
}

auto SessionManager::BeginFinalization(SessionEndReason reason, bool recording_only) -> bool {
    if (Phase() == SessionPhase::kFinalizing) {
        return false;
    }
    WaitForFinalization();
    std::lock_guard lock(mtx_);
    if (state_.phase == SessionPhase::kIdle || state_.phase == SessionPhase::kFinalizing ||
        (recording_only && state_.phase != SessionPhase::kRecording) ||
        (reason == SessionEndReason::kAutoStop &&
         (!disconnected_at_ ||
          now_() - *disconnected_at_ < std::chrono::minutes(state_.config
                                                                ? state_.config->auto_stop_minutes
                                                                : kDefaultAutoStopMinutes)))) {
        return false;
    }
    AdvanceMatchClock();
    SessionState ending = state_;
    state_.phase = SessionPhase::kFinalizing;
    recording_only_finalizing_ = recording_only;
    recording_started_at_.reset();
    disconnected_at_.reset();
    {
        std::lock_guard worker_lock(worker_mtx_);
        // A non-finalizing phase means the old worker released mtx_ and is exiting.
        if (finalize_worker_.joinable()) {
            finalize_worker_.join();
        }
        finalize_worker_ = std::thread([this, ending = std::move(ending), reason, recording_only] {
            FinishSession(ending, reason, recording_only);
        });
    }
    return true;
}

auto SessionManager::FinishSession(SessionState ending, SessionEndReason reason,
                                   bool recording_only) -> void {
    bool file_valid = ending.phase != SessionPhase::kRecording && ending.last_session &&
                      ending.last_session->file_valid;
    const auto safe = [](const char* step, auto&& action) {
        try {
            action();
        } catch (const std::exception& e) {
            spdlog::error("SessionManager: cleanup step {} threw: {}", step, e.what());
        } catch (...) {
            spdlog::error("SessionManager: cleanup step {} threw (non-std)", step);
        }
    };
    safe("FinalizeRecording", [this, &file_valid, &ending] {
        if (ending.phase == SessionPhase::kRecording) {
            file_valid = cleanup_.FinalizeRecording();
        }
    });
    std::unique_lock lock(mtx_);
    const bool end = !recording_only || !state_.is_streaming;
    if (end) {
        finalization_committing_ = true;
        lock.unlock();
        safe("StopStreaming", [this] { cleanup_.StopStreaming(); });
        safe("TeardownWifiDirect", [this] { cleanup_.TeardownWifiDirect(); });
        safe("ResetSelections", [this] { cleanup_.ResetSelections(); });
        lock.lock();
        state_ = SessionState{};
        match_clock_started_at_.reset();
    } else {
        state_.phase = SessionPhase::kReady;
        state_.recording_elapsed_seconds.reset();
    }
    recording_only_finalizing_ = false;
    finalization_committing_ = false;
    if (ending.config) {
        if (end && reason == SessionEndReason::kShutdown && file_valid && !video_root_.empty()) {
            file_valid = PersistStopSummary(LastSession{ending.config->match_uuid, reason,
                                                        ending.match.clock_seconds, file_valid});
        }
        if (!video_root_.empty() && !ending.config->video_output_path.empty() && file_valid) {
            std::error_code error;
            fs::remove(ActiveMarkerPath(ending.config->video_output_path), error);
        }
        state_.last_session =
            LastSession{ending.config->match_uuid, reason, ending.match.clock_seconds, file_valid};
    }
    recording_started_at_.reset();
}

auto SessionManager::PersistStopSummary(const LastSession& summary) -> bool {
    if (video_root_.empty()) {
        return false;
    }
    const auto target = video_root_ / ".last-session-shutdown";
    const auto temporary = video_root_ / ".last-session-shutdown.tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << summary.match_uuid << '\n' << summary.clock_seconds << '\n';
    if (summary.reason == SessionEndReason::kAppStop) {
        file << "app-stop\n";
    }
    file.close();
    std::error_code error;
    if (file) {
        fs::rename(temporary, target, error);
    }
    if (!file || error) {
        spdlog::error("SessionManager: failed to persist stop summary: {}",
                      error ? error.message() : "write failed");
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

auto SessionManager::ApplyMatchUpdate(const std::function<void(LiveMatch&)>& mutate) -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase < SessionPhase::kConfigured || state_.phase == SessionPhase::kFinalizing ||
        !mutate) {
        return false;
    }
    AdvanceMatchClock();
    const auto previous_seconds = state_.match.clock_seconds;
    const bool was_running = state_.match.clock_running;
    mutate(state_.match);
    if (!state_.match.clock_running) {
        match_clock_started_at_.reset();
    } else if (!was_running || state_.match.clock_seconds != previous_seconds) {
        match_clock_started_at_ = now_();
    }
    return true;
}

auto SessionManager::ReconcileMatch(const std::function<void(LiveMatch&)>& mutate) -> bool {
    std::lock_guard lock(mtx_);
    if (state_.phase < SessionPhase::kConfigured || state_.phase == SessionPhase::kFinalizing ||
        !mutate) {
        return false;
    }
    AdvanceMatchClock();
    mutate(state_.match);
    match_clock_started_at_ = state_.match.clock_running ? std::optional{now_()} : std::nullopt;
    return true;
}

auto SessionManager::AdvanceMatchClock() -> void {
    if (match_clock_started_at_) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now_() - *match_clock_started_at_);
        if (elapsed.count() > 0) {
            state_.match.clock_seconds += static_cast<std::uint32_t>(elapsed.count());
            *match_clock_started_at_ += elapsed;
        }
    }
}

auto SessionManager::PrepareOutputDirs(const SessionConfig& config) -> bool {
    const auto make_parent = [](const std::string& file_path) -> bool {
        if (file_path.empty()) {
            return true;
        }
        const fs::path parent = fs::path{file_path}.parent_path();
        if (parent.empty()) {
            return true;
        }
        std::error_code error;
        fs::create_directories(parent, error);
        if (error) {
            spdlog::error("SessionManager: mkdir({}) failed: {}", parent.string(), error.message());
            return false;
        }
        return true;
    };

    return make_parent(config.video_output_path) && make_parent(config.thumbnail_output_path);
}

}  // namespace sst::session
