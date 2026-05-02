#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "app/buffer/ports/frame-sink.hpp"
#include "app/db/ports/recording-repository.hpp"
#include "app/storage/ports/disk-guard.hpp"
#include "app/storage/ports/encoder-pipeline.hpp"
#include "app/storage/ports/event-clip-recorder.hpp"
#include "app/storage/ports/recording-service.hpp"
#include "app/storage/ports/segment-recorder.hpp"
#include "domain/storage/models/event-clip-window.hpp"
#include "domain/storage/models/recording-state.hpp"

namespace sst::storage {

// Owns the recording state machine, the shared encoder pipeline, the segment
// recorder (full-game) and the event-clip recorder (independent mp4 per
// match-event). Implements IFrameSink so the orchestration thread pushes
// post-processed frames straight in via a single Push call.
//
// Threading: Push() runs on the producer thread (orchestration), the lifecycle
// methods (StartFullGame / Pause / Resume / StopFullGame / TriggerEventClip)
// run on the BLE thread. Both share `mtx_`; Push() takes it briefly to read
// the current state before forwarding to the encoder.
class RecordingService final : public IRecordingService, public sst::buffer::IFrameSink {
   public:
    RecordingService(std::unique_ptr<IEncoderPipeline> encoder,
                     std::unique_ptr<ISegmentRecorder> segment_recorder,
                     std::unique_ptr<IEventClipRecorder> event_clip_recorder,
                     sst::db::IRecordingRepository& recording_repo, IDiskGuard& disk_guard,
                     std::filesystem::path video_root);

    ~RecordingService() override;

    RecordingService(const RecordingService&) = delete;
    auto operator=(const RecordingService&) -> RecordingService& = delete;
    RecordingService(RecordingService&&) = delete;
    auto operator=(RecordingService&&) -> RecordingService& = delete;

    // IRecordingService
    auto StartFullGame(const std::string& match_id) -> StartFullGameResult override;
    auto Pause() -> bool override;
    auto Resume() -> bool override;
    auto StopFullGame() -> StopFullGameResult override;
    auto TriggerEventClip(const std::string& match_event_id, const EventClipWindow& window)
        -> TriggerEventClipResult override;
    [[nodiscard]] auto CurrentState() const -> RecordingState override;

    // IFrameSink
    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    // Wires the encoder's tee outputs to the two consumer adapters. Called once
    // at construction.
    auto ConnectEncoderSinks() -> void;

    static auto NowIso8601() -> std::string;
    static auto MatchDir(const std::filesystem::path& root, const std::string& match_id)
        -> std::filesystem::path;
    static auto FullGameDir(const std::filesystem::path& root, const std::string& match_id)
        -> std::filesystem::path;
    static auto EventClipDir(const std::filesystem::path& root, const std::string& match_id,
                             const std::string& event_clip_id) -> std::filesystem::path;

    std::unique_ptr<IEncoderPipeline> encoder_;
    std::unique_ptr<ISegmentRecorder> segment_recorder_;
    std::unique_ptr<IEventClipRecorder> event_clip_recorder_;
    sst::db::IRecordingRepository& recording_repo_;
    IDiskGuard& disk_guard_;
    std::filesystem::path video_root_;

    mutable std::mutex mtx_;
    RecordingState state_{RecordingState::kIdle};
    std::string active_match_id_;
    std::string active_recording_id_;
};

}  // namespace sst::storage
