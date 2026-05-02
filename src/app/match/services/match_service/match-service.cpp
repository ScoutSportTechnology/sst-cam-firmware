#include "app/match/services/match_service/match-service.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#include <spdlog/spdlog.h>

#include "domain/common/utils/uuid.hpp"
#include "domain/db/models/match.hpp"
#include "domain/db/models/sport.hpp"
#include "domain/storage/models/event-clip-window.hpp"

namespace sst::match {

MatchService::MatchService(sst::db::IMatchRepository& match_repo,
                           sst::db::ISportRepository& sport_repo,
                           sst::db::ICameraRepository& camera_repo,
                           sst::storage::IRecordingService& recording_service,
                           std::int64_t user_id)
    : match_repo_(match_repo),
      sport_repo_(sport_repo),
      camera_repo_(camera_repo),
      recording_service_(recording_service),
      user_id_(user_id) {}

auto MatchService::StartMatch(const StartMatchRequest& request) -> StartMatchResult {
    std::lock_guard lock(mtx_);
    if (!active_match_id_.empty()) {
        spdlog::warn("MatchService::StartMatch rejected: match {} already active",
                     active_match_id_);
        return {};
    }
    if (request.team_a_id == request.team_b_id) {
        spdlog::warn("MatchService::StartMatch rejected: same team for both sides ({})",
                     request.team_a_id);
        return {};
    }

    auto sport = sport_repo_.getByCode(request.sport_code);
    if (!sport.success) {
        spdlog::warn("MatchService::StartMatch rejected: unknown sport_code \"{}\"",
                     request.sport_code);
        return {};
    }

    const std::string match_id = sst::common::utils::MakeUuidV4();
    const std::string started_at = NowIso8601();

    sst::db::Match row;
    row.id = match_id;
    row.user_id = user_id_;
    row.sport_id = sport.data.id;
    row.team_a_id = request.team_a_id;
    row.team_b_id = request.team_b_id;
    row.name = request.name;
    row.started_at = started_at;
    row.current_period = sst::db::Match::kDefaultCurrentPeriod;
    row.final_score_a = 0;
    row.final_score_b = 0;

    if (!match_repo_.save(row).success) {
        // Composite FK on (team_id, sport_id) rejects mismatched sport+teams,
        // and the CHECK on team_a_id != team_b_id is also enforced by the DB.
        spdlog::warn("MatchService::StartMatch: DB rejected match save (FK / CHECK violation)");
        return {};
    }

    auto recording = recording_service_.StartFullGame(match_id);
    if (!recording.success) {
        spdlog::error("MatchService::StartMatch: recording service refused start, rolling back");
        match_repo_.remove(match_id);
        return {};
    }

    active_match_id_ = match_id;
    active_sport_id_ = sport.data.id;
    current_period_ = sst::db::Match::kDefaultCurrentPeriod;
    score_a_ = 0;
    score_b_ = 0;
    match_start_clock_ = std::chrono::steady_clock::now();

    spdlog::info("MatchService::StartMatch OK match={} sport={} a={} b={}", match_id,
                 sport.data.id, request.team_a_id, request.team_b_id);
    return {.success = true, .match_id = match_id, .recording_id = recording.recording_id};
}

auto MatchService::SetPeriod(std::int32_t period) -> bool {
    std::lock_guard lock(mtx_);
    if (active_match_id_.empty()) {
        return false;
    }
    if (period <= 0) {
        return false;
    }
    if (!match_repo_.updatePeriod(active_match_id_, period).success) {
        return false;
    }
    current_period_ = period;
    return true;
}

auto MatchService::SetScore(const SetScoreRequest& request) -> bool {
    std::lock_guard lock(mtx_);
    if (active_match_id_.empty()) {
        return false;
    }
    if (request.score < 0) {
        return false;
    }
    if (request.team == TeamSide::kA) {
        score_a_ = request.score;
    } else {
        score_b_ = request.score;
    }
    return match_repo_.updateScore(active_match_id_, score_a_, score_b_).success;
}

auto MatchService::RecordEvent(const RecordEventRequest& request) -> RecordEventResult {
    std::lock_guard lock(mtx_);
    if (active_match_id_.empty()) {
        spdlog::warn("MatchService::RecordEvent rejected: no active match");
        return {};
    }

    // Validate event_code is known for the active sport. The DB FK on
    // (sport_id, event_code) → sport_event_kind would also catch this on
    // insert, but failing fast here gives a clearer BLE error.
    auto kind = sport_repo_.getEventKind(active_sport_id_, request.event_code);
    if (!kind.success) {
        spdlog::warn("MatchService::RecordEvent rejected: event_code \"{}\" not valid for sport_id={}",
                     request.event_code, active_sport_id_);
        return {};
    }

    auto camera_cfg = camera_repo_.getConfig(user_id_);
    if (!camera_cfg.success) {
        spdlog::error("MatchService::RecordEvent: camera_config row missing for user_id={}",
                      user_id_);
        return {};
    }
    const sst::storage::EventClipWindow window{
        .pre_seconds = camera_cfg.data.event_clip_pre_seconds,
        .post_seconds = camera_cfg.data.event_clip_post_seconds,
    };

    sst::db::MatchEvent ev;
    ev.id = sst::common::utils::MakeUuidV4();
    ev.match_id = active_match_id_;
    ev.sport_id = active_sport_id_;
    ev.event_code = request.event_code;
    ev.period = current_period_;
    ev.timestamp_in_match = SecondsSince(match_start_clock_);
    ev.wall_clock_at = NowIso8601();
    ev.metadata_json = request.metadata_json;
    if (!match_repo_.saveEvent(ev).success) {
        spdlog::error("MatchService::RecordEvent: DB save (match_event) failed");
        return {};
    }

    auto clip = recording_service_.TriggerEventClip(ev.id, window);
    if (!clip.success) {
        // Recording is the artifact; the match_event row stays (still a valid
        // record of what happened) — only the clip file failed to start.
        spdlog::warn("MatchService::RecordEvent: clip recorder rejected trigger; event_id={}",
                     ev.id);
        return {};
    }

    return {.success = true,
            .match_event_id = ev.id,
            .event_clip_id = clip.event_clip_id,
            .file_path = clip.file_path};
}

auto MatchService::EndMatch() -> EndMatchResult {
    std::lock_guard lock(mtx_);
    if (active_match_id_.empty()) {
        return {};
    }

    auto stop = recording_service_.StopFullGame();
    const std::string ended_at = NowIso8601();
    if (!match_repo_.finalize(active_match_id_, ended_at).success) {
        spdlog::warn("MatchService::EndMatch: DB finalize failed for match={}", active_match_id_);
    }

    spdlog::info("MatchService::EndMatch OK match={} merged={}", active_match_id_,
                 stop.merged_path.string());

    active_match_id_.clear();
    active_sport_id_ = 0;
    current_period_ = 1;
    score_a_ = 0;
    score_b_ = 0;
    return {.success = stop.success, .merged_path = stop.merged_path};
}

auto MatchService::IsActive() const -> bool {
    std::lock_guard lock(mtx_);
    return !active_match_id_.empty();
}

auto MatchService::ActiveMatchId() const -> std::string {
    std::lock_guard lock(mtx_);
    return active_match_id_;
}

auto MatchService::NowIso8601() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

auto MatchService::SecondsSince(std::chrono::steady_clock::time_point t0) -> double {
    const auto delta = std::chrono::steady_clock::now() - t0;
    return std::chrono::duration<double>(delta).count();
}

}  // namespace sst::match
