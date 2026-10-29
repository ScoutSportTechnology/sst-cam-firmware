#include "app/control/services/handlers/match.handler.hpp"

#include <spdlog/spdlog.h>

#include <map>
#include <string>
#include <utility>

#include "app/overlay/binding-sync.hpp"
#include "domain/session/models/live-match.hpp"

namespace sst::control {

using sst::session::LiveMatch;
using sst::session::MatchSegment;

MatchHandler::MatchHandler(sst::session::ISessionManager& session,
                           sst::overlay::OverlayController& controller, Clock now_ms)
    : session_(session), controller_(controller), now_ms_(std::move(now_ms)) {}

auto MatchHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kMatchControl, sst_cam::Command::kScoreUpdate,
            sst_cam::Command::kBannerEvent, sst_cam::Command::kSetMatchState};
}

auto MatchHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    switch (cmd.payload_case()) {
        case sst_cam::Command::kScoreUpdate:
            return HandleScoreUpdate(cmd.score_update());
        case sst_cam::Command::kMatchControl:
            return HandleMatchControl(cmd.match_control());
        case sst_cam::Command::kBannerEvent:
            return HandleBannerEvent(cmd.banner_event());
        case sst_cam::Command::kSetMatchState:
            return HandleSetMatchState(cmd.set_match_state());
        default:
            break;
    }
    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::ERROR);
    resp.set_error_message("match handler: unexpected command");
    return resp;
}

auto MatchHandler::SyncOverlay() -> void {
    controller_.SetBindingData(sst::overlay::BuildBindingData(session_.Snapshot()));
    controller_.Refresh(now_ms_ ? now_ms_() : 0);
}

auto MatchHandler::HandleSetMatchState(const sst_cam::SetMatchStateCommand& cmd)
    -> sst_cam::CommandResponse {
    const bool applied = session_.ReconcileMatch([&cmd](LiveMatch& match) {
        if (cmd.has_score_a()) {
            match.score_a = cmd.score_a();
        }
        if (cmd.has_score_b()) {
            match.score_b = cmd.score_b();
        }
        if (cmd.has_current_period()) {
            match.period = cmd.current_period();
        }
        if (cmd.has_elapsed_seconds()) {
            match.clock_seconds = cmd.elapsed_seconds();
        }
        if (cmd.has_clock_running()) {
            match.clock_running = cmd.clock_running();
        }
        if (cmd.has_status()) {
            match.status_override.reset();
            switch (cmd.status()) {
                case sst_cam::MATCH_HALF_TIME:
                    match.segment = MatchSegment::kHalfTime;
                    break;
                case sst_cam::MATCH_FINISHED:
                    match.segment = MatchSegment::kFullTime;
                    break;
                case sst_cam::MATCH_ACTIVE:
                case sst_cam::MATCH_PAUSED:
                case sst_cam::MATCH_NOT_STARTED:
                case sst_cam::MATCH_STATUS_UNKNOWN:
                    match.status_override = static_cast<std::uint8_t>(cmd.status());
                    match.segment = MatchSegment::kInPlay;
                    break;
            }
        }
    });
    sst_cam::CommandResponse resp;
    resp.set_status(applied ? sst_cam::OK : sst_cam::ERROR);
    if (applied) {
        if (cmd.has_status() && cmd.status() == sst_cam::MATCH_FINISHED) {
            controller_.Clear();
        } else {
            SyncOverlay();
        }
    } else {
        resp.set_error_message("match state rejected: no active session");
    }
    return resp;
}

auto MatchHandler::HandleScoreUpdate(const sst_cam::ScoreUpdateCommand& cmd)
    -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;
    const auto state = session_.Snapshot();
    if (!state.config) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("score update with no active session");
        return resp;
    }
    const bool is_home = cmd.team_id() == state.config->team_a_id;
    const bool is_away = cmd.team_id() == state.config->team_b_id;
    if (!is_home && !is_away) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("score update: unknown team_id");
        return resp;
    }

    const std::int32_t delta = cmd.delta();
    const bool applied = session_.ApplyMatchUpdate([&](LiveMatch& match) {
        std::uint32_t& score = is_home ? match.score_a : match.score_b;
        // Clamp at zero — a negative delta can't drive the score below 0.
        if (delta < 0 && static_cast<std::uint32_t>(-delta) > score) {
            score = 0;
        } else {
            score = static_cast<std::uint32_t>(static_cast<std::int64_t>(score) + delta);
        }
    });
    if (!applied) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("score update rejected: no active session");
        return resp;
    }

    SyncOverlay();
    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

auto MatchHandler::HandleMatchControl(const sst_cam::MatchControlCommand& cmd)
    -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;
    const std::uint32_t period = cmd.period();
    const auto action = cmd.action();

    const bool applied = session_.ApplyMatchUpdate([&](LiveMatch& match) {
        switch (action) {
            case sst_cam::MATCH_KICKOFF:
                match.status_override.reset();
                match.period = period > 0 ? period : 1;
                match.clock_seconds = 0;
                match.clock_running = true;
                match.segment = MatchSegment::kInPlay;
                break;
            case sst_cam::MATCH_PERIOD_START:
                match.status_override.reset();
                match.period = period > 0 ? period : match.period;
                // A new period starts from 00:00 — reset the clock (KICKOFF does
                // this too; PERIOD_START previously carried the prior period's
                // time into the overlay).
                match.clock_seconds = 0;
                match.clock_running = true;
                match.segment = MatchSegment::kInPlay;
                break;
            case sst_cam::MATCH_PERIOD_END:
                match.status_override.reset();
                match.clock_running = false;
                match.segment = MatchSegment::kHalfTime;
                break;
            case sst_cam::MATCH_FINAL_WHISTLE:
                match.status_override.reset();
                match.clock_running = false;
                match.segment = MatchSegment::kFullTime;
                break;
            case sst_cam::MATCH_CLOCK_PAUSE:
                match.status_override.reset();
                match.clock_running = false;
                break;
            case sst_cam::MATCH_CLOCK_RESUME:
                match.status_override.reset();
                match.clock_running = true;
                break;
            default:
                break;
        }
    });
    if (!applied) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("match control rejected: no active session");
        return resp;
    }

    if (action == sst_cam::MATCH_FINAL_WHISTLE) {
        // Match is over — remove the scoreboard from the live preview entirely
        // (no active match => no overlay), rather than leaving a frozen FT board.
        spdlog::info("MatchHandler: final whistle -> clearing overlay");
        controller_.Clear();
    } else {
        SyncOverlay();
    }
    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

auto MatchHandler::HandleBannerEvent(const sst_cam::BannerEventCommand& cmd)
    -> sst_cam::CommandResponse {
    std::map<std::string, std::string> params;
    for (const auto& [key, value] : cmd.params()) {
        params.emplace(key, value);
    }
    const std::uint64_t now = now_ms_ ? now_ms_() : 0;
    // Unknown template_id is a no-op (the app may target a template this layout
    // doesn't define) — still a successful, defined response.
    const bool shown = controller_.ActivateBanner(cmd.template_id(), params, cmd.duration_s(), now);
    controller_.Refresh(now);
    spdlog::info("MatchHandler: banner '{}' duration_s={} activated={}", cmd.template_id(),
                 cmd.duration_s(), shown);

    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

auto MatchHandler::TickClock() -> void {
    bool ticked = false;
    session_.ApplyMatchUpdate([&ticked](LiveMatch& match) { ticked = match.clock_running; });
    if (ticked) {
        SyncOverlay();
    }
}

}  // namespace sst::control
