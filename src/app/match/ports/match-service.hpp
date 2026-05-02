#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace sst::match {

struct StartMatchRequest {
    std::string sport_code;     // e.g. "soccer"
    std::int64_t team_a_id{0};
    std::int64_t team_b_id{0};
    std::optional<std::string> name;
};

struct StartMatchResult {
    bool success{false};
    std::string match_id;       // UUID of the freshly-inserted match row
    std::string recording_id;   // UUID of the full-game recording row
};

enum class TeamSide : std::uint8_t { kA = 0, kB = 1 };

struct SetScoreRequest {
    TeamSide team{TeamSide::kA};
    std::int32_t score{0};
};

struct RecordEventRequest {
    std::string event_code;     // must be a valid sport_event_kind for the active match's sport
    std::optional<std::string> metadata_json;
};

struct RecordEventResult {
    bool success{false};
    std::string match_event_id;     // UUID of the inserted match_event row
    std::string event_clip_id;      // UUID of the inserted event_clip row
    std::filesystem::path file_path;
};

struct EndMatchResult {
    bool success{false};
    std::filesystem::path merged_path;  // path to the concatenated full-game mp4
};

// Match lifecycle facade. The BLE MatchController routes commands here. The
// service tracks at most one active match at a time (single-camera firmware,
// one user) and delegates recording to sst::storage::IRecordingService.
class IMatchService {
   public:
    virtual ~IMatchService() = default;

    virtual auto StartMatch(const StartMatchRequest& request) -> StartMatchResult = 0;
    virtual auto SetPeriod(std::int32_t period) -> bool = 0;
    virtual auto SetScore(const SetScoreRequest& request) -> bool = 0;
    virtual auto RecordEvent(const RecordEventRequest& request) -> RecordEventResult = 0;
    virtual auto EndMatch() -> EndMatchResult = 0;

    [[nodiscard]] virtual auto IsActive() const -> bool = 0;
    [[nodiscard]] virtual auto ActiveMatchId() const -> std::string = 0;
};

}  // namespace sst::match
