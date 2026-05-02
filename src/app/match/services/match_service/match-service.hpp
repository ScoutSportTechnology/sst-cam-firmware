#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include "app/db/ports/camera-repository.hpp"
#include "app/db/ports/match-repository.hpp"
#include "app/db/ports/sport-repository.hpp"
#include "app/match/ports/match-service.hpp"
#include "app/storage/ports/recording-service.hpp"

namespace sst::match {

// Owns the match state machine. Tracks at most one active match. Coordinates
// the DB (match / match_event rows + score/period updates) and the recording
// service (full-game record on StartMatch, event clip on RecordEvent, segment
// merge on EndMatch). The caller is the BLE MatchController.
//
// Pre/post seconds for event clips are read from camera_config at trigger
// time so user edits to those fields take effect on the next event.
class MatchService final : public IMatchService {
   public:
    MatchService(sst::db::IMatchRepository& match_repo, sst::db::ISportRepository& sport_repo,
                 sst::db::ICameraRepository& camera_repo,
                 sst::storage::IRecordingService& recording_service, std::int64_t user_id);

    auto StartMatch(const StartMatchRequest& request) -> StartMatchResult override;
    auto SetPeriod(std::int32_t period) -> bool override;
    auto SetScore(const SetScoreRequest& request) -> bool override;
    auto RecordEvent(const RecordEventRequest& request) -> RecordEventResult override;
    auto EndMatch() -> EndMatchResult override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto ActiveMatchId() const -> std::string override;

   private:
    static auto NowIso8601() -> std::string;
    static auto SecondsSince(std::chrono::steady_clock::time_point t0) -> double;

    sst::db::IMatchRepository& match_repo_;
    sst::db::ISportRepository& sport_repo_;
    sst::db::ICameraRepository& camera_repo_;
    sst::storage::IRecordingService& recording_service_;
    std::int64_t user_id_;

    mutable std::mutex mtx_;
    std::string active_match_id_;
    std::int64_t active_sport_id_{0};
    std::int32_t current_period_{1};
    std::int32_t score_a_{0};
    std::int32_t score_b_{0};
    std::chrono::steady_clock::time_point match_start_clock_{};
};

}  // namespace sst::match
