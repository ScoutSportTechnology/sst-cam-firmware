#include "app/storage/services/recording_service/recording-service.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

#include "domain/common/utils/uuid.hpp"
#include "domain/storage/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::storage {

namespace {

constexpr const char* kFullGameDirName = "full_game";
constexpr const char* kEventDirPrefix = "event_";
constexpr const char* kEventClipFile = "clip.mp4";

}  // namespace

RecordingService::RecordingService(std::unique_ptr<IEncoderPipeline> encoder,
                                   std::unique_ptr<ISegmentRecorder> segment_recorder,
                                   std::unique_ptr<IEventClipRecorder> event_clip_recorder,
                                   IDiskGuard& disk_guard,
                                   std::filesystem::path video_root)
    : encoder_(std::move(encoder)),
      segment_recorder_(std::move(segment_recorder)),
      event_clip_recorder_(std::move(event_clip_recorder)),
      disk_guard_(disk_guard),
      video_root_(std::move(video_root)) {
    ConnectEncoderSinks();
}

RecordingService::~RecordingService() {
    if (state_ != RecordingState::kIdle) {
        spdlog::warn("RecordingService: destructor while state={} — forcing stop", state_);
        (void)StopFullGame();
    }
}

auto RecordingService::ConnectEncoderSinks() -> void {
    encoder_->SetSegmentSink(
        [this](const EncodedNal& nal) { segment_recorder_->OnEncoded(nal); });
    encoder_->SetRingSink(
        [this](const EncodedNal& nal) { event_clip_recorder_->OnEncoded(nal); });
}

auto RecordingService::StartFullGame(const std::string& match_id) -> StartFullGameResult {
    std::lock_guard lock(mtx_);
    if (state_ != RecordingState::kIdle) {
        spdlog::warn("RecordingService::StartFullGame rejected: state={} active_match={}", state_,
                     active_match_id_);
        return {.success = false, .recording_id = {}};
    }
    if (!disk_guard_.HasEnoughFreeSpace()) {
        spdlog::warn(
            "RecordingService::StartFullGame rejected: disk guard refused (free={} bytes)",
            disk_guard_.FreeBytes());
        return {.success = false, .recording_id = {}};
    }

    const auto dir = FullGameDir(video_root_, match_id);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::error("RecordingService::StartFullGame: mkdir({}) failed: {}", dir.string(),
                      ec.message());
        return {.success = false, .recording_id = {}};
    }

    const std::string recording_id = sst::common::utils::MakeUuidV4();
    const std::string started_at = NowIso8601();
    const auto merged_path = dir / "full_game.mp4";

    // Filesystem cleanup on any post-mkdir rollback path. Removes the full_game
    // subdir we just created, and the parent <match_uuid> dir too if it ended
    // up empty (it will, since this is a fresh UUID).
    auto cleanup_dirs = [&] {
        std::error_code rec;
        std::filesystem::remove_all(dir, rec);
        std::filesystem::remove(MatchDir(video_root_, match_id), rec);
    };

    // Recording metadata is no longer persisted to a local DB (app is the source
    // of truth); recordings are enumerated from disk on demand (U13).
    (void)merged_path;
    (void)started_at;

    if (!encoder_->Start()) {
        spdlog::error("RecordingService::StartFullGame: encoder failed to start");
        cleanup_dirs();
        return {.success = false, .recording_id = {}};
    }

    if (!segment_recorder_->Start(dir)) {
        spdlog::error("RecordingService::StartFullGame: segment recorder failed to start");
        encoder_->Stop();
        cleanup_dirs();
        return {.success = false, .recording_id = {}};
    }

    state_ = RecordingState::kRecording;
    active_match_id_ = match_id;
    active_recording_id_ = recording_id;
    spdlog::info("RecordingService: StartFullGame match={} recording={} dir={}", match_id,
                 recording_id, dir.string());
    return {.success = true, .recording_id = recording_id};
}

auto RecordingService::Pause() -> bool {
    std::lock_guard lock(mtx_);
    if (state_ != RecordingState::kRecording) {
        spdlog::warn("RecordingService::Pause rejected: state={}", state_);
        return false;
    }
    segment_recorder_->Pause();
    state_ = RecordingState::kPaused;
    spdlog::info("RecordingService: Paused (encoder still running for event-clip ring)");
    return true;
}

auto RecordingService::Resume() -> bool {
    std::lock_guard lock(mtx_);
    if (state_ != RecordingState::kPaused) {
        spdlog::warn("RecordingService::Resume rejected: state={}", state_);
        return false;
    }
    segment_recorder_->Resume();
    state_ = RecordingState::kRecording;
    spdlog::info("RecordingService: Resumed");
    return true;
}

auto RecordingService::StopFullGame() -> StopFullGameResult {
    std::lock_guard lock(mtx_);
    if (state_ == RecordingState::kIdle) {
        spdlog::warn("RecordingService::StopFullGame rejected: state=Idle");
        return {.success = false, .merged_path = {}};
    }

    const auto merged = segment_recorder_->Stop();
    encoder_->Stop();

    spdlog::info("RecordingService: StopFullGame match={} merged={}", active_match_id_,
                 merged.string());

    state_ = RecordingState::kIdle;
    active_match_id_.clear();
    active_recording_id_.clear();

    return {.success = !merged.empty(), .merged_path = merged};
}

auto RecordingService::TriggerEventClip(const std::string& match_event_id,
                                        const EventClipWindow& window)
    -> TriggerEventClipResult {
    std::lock_guard lock(mtx_);
    if (state_ == RecordingState::kIdle) {
        spdlog::warn("RecordingService::TriggerEventClip rejected: state=Idle");
        return {};
    }
    if (window.pre_seconds <= 0 || window.post_seconds <= 0) {
        spdlog::error("RecordingService::TriggerEventClip rejected: invalid window {}", window);
        return {};
    }
    if (!disk_guard_.HasEnoughFreeSpace()) {
        spdlog::warn(
            "RecordingService::TriggerEventClip rejected: disk guard refused (free={} bytes)",
            disk_guard_.FreeBytes());
        return {};
    }

    const std::string event_clip_id = sst::common::utils::MakeUuidV4();
    const std::string clip_recording_id = sst::common::utils::MakeUuidV4();
    (void)match_event_id;  // No DB row linkage; clip is identified by its file path.
    const auto dir = EventClipDir(video_root_, active_match_id_, event_clip_id);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::error("RecordingService::TriggerEventClip: mkdir({}) failed: {}", dir.string(),
                      ec.message());
        return {};
    }

    const auto file_path = dir / kEventClipFile;

    // Removes the event_<uuid> dir we just created. The parent <match_uuid>
    // is left alone here — the active full-game recording is still writing
    // into it.
    auto cleanup_dir = [&] {
        std::error_code rec;
        std::filesystem::remove_all(dir, rec);
    };

    if (!event_clip_recorder_->Trigger(file_path, window)) {
        spdlog::error("RecordingService::TriggerEventClip: recorder rejected trigger");
        cleanup_dir();
        return {};
    }

    spdlog::info("RecordingService: TriggerEventClip match={} event={} clip={} window={}",
                 active_match_id_, match_event_id, event_clip_id, window);

    return {.success = true,
            .event_clip_id = event_clip_id,
            .recording_id = clip_recording_id,
            .file_path = file_path};
}

auto RecordingService::CurrentState() const -> RecordingState {
    std::lock_guard lock(mtx_);
    return state_;
}

auto RecordingService::Push(const sst::capture::Frame& frame) -> void {
    {
        std::lock_guard lock(mtx_);
        if (state_ == RecordingState::kIdle) {
            return;
        }
    }
    encoder_->Push(frame);
}

auto RecordingService::NowIso8601() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

auto RecordingService::MatchDir(const std::filesystem::path& root, const std::string& match_id)
    -> std::filesystem::path {
    return root / match_id;
}

auto RecordingService::FullGameDir(const std::filesystem::path& root,
                                   const std::string& match_id) -> std::filesystem::path {
    return MatchDir(root, match_id) / kFullGameDirName;
}

auto RecordingService::EventClipDir(const std::filesystem::path& root,
                                    const std::string& match_id,
                                    const std::string& event_clip_id) -> std::filesystem::path {
    return MatchDir(root, match_id) / (std::string{kEventDirPrefix} + event_clip_id);
}

}  // namespace sst::storage
