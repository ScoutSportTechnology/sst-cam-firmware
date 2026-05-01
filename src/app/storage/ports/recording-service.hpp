#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "domain/storage/models/event-clip-window.hpp"
#include "domain/storage/models/recording-state.hpp"

namespace sst::storage {

struct StartFullGameResult {
    bool success{false};
    std::string recording_id;  // UUID of the newly-created recording row
};

struct StopFullGameResult {
    bool success{false};
    std::filesystem::path merged_path;
};

struct TriggerEventClipResult {
    bool success{false};
    std::string event_clip_id;     // UUID of the new event_clip row
    std::string recording_id;      // UUID of the kEventClip recording row
    std::filesystem::path file_path;
};

// Sole entry point for the BLE controllers and the orchestration thread.
// Implements IFrameSink internally so postprocess output goes straight in.
class IRecordingService {
   public:
    virtual ~IRecordingService() = default;

    // Idempotent on `match_id` — a second StartFullGame for the same match
    // while one is already running fails with success=false.
    virtual auto StartFullGame(const std::string& match_id) -> StartFullGameResult = 0;

    virtual auto Pause() -> bool = 0;
    virtual auto Resume() -> bool = 0;

    virtual auto StopFullGame() -> StopFullGameResult = 0;

    // pre/post seconds are looked up by the caller (MatchController) from
    // camera_config so live edits to those fields take effect per-trigger.
    virtual auto TriggerEventClip(const std::string& match_event_id,
                                  const EventClipWindow& window) -> TriggerEventClipResult = 0;

    [[nodiscard]] virtual auto CurrentState() const -> RecordingState = 0;
};

}  // namespace sst::storage
