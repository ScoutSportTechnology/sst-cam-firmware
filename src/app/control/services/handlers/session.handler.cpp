#include "app/control/services/handlers/session.handler.hpp"

#include <utility>

#include "domain/session/models/session-config.hpp"

namespace sst::control {

namespace {
auto FillMatchState(const sst::session::SessionState& state, sst_cam::MatchState* match) -> void {
    if (!state.config) {
        return;
    }
    const auto& config = state.config.value();
    match->set_match_uuid(config.match_uuid);
    match->set_current_period(state.match.period);
    match->set_score_a(state.match.score_a);
    match->set_score_b(state.match.score_b);
    match->set_team_a_id(config.team_a_id);
    match->set_team_b_id(config.team_b_id);
    match->set_elapsed_seconds(state.match.clock_seconds);
    match->set_clock_running(state.match.clock_running);
    const auto length = config.period_length_seconds;
    match->set_time_remaining_s(length > 0 && state.match.clock_seconds <
                                                  static_cast<std::uint32_t>(length)
                                    ? static_cast<std::uint32_t>(length) - state.match.clock_seconds
                                    : 0);
    if (state.match.segment == sst::session::MatchSegment::kFullTime) {
        match->set_status(sst_cam::MATCH_FINISHED);
    } else if (state.match.segment == sst::session::MatchSegment::kHalfTime) {
        match->set_status(sst_cam::MATCH_HALF_TIME);
    } else if (state.match.period == 0) {
        match->set_status(sst_cam::MATCH_NOT_STARTED);
    } else {
        match->set_status(state.match.clock_running ? sst_cam::MATCH_ACTIVE
                                                    : sst_cam::MATCH_PAUSED);
    }
}
}  // namespace

SessionHandler::SessionHandler(
    sst::session::ISessionManager& session, sst::overlay::OverlayController& controller,
    std::function<std::optional<std::uint32_t>()> camera,
    std::function<std::optional<sst_cam::PreviewLayout>()> layout,
    std::function<std::optional<sst_cam::CameraHealth>(std::uint32_t)> health)
    : session_(session),
      controller_(controller),
      camera_(std::move(camera)),
      layout_(std::move(layout)),
      health_(std::move(health)) {}

auto SessionHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kPushSessionConfig, sst_cam::Command::kGetSessionSnapshot};
}

auto SessionHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    if (cmd.payload_case() == sst_cam::Command::kGetSessionSnapshot) {
        return SnapshotResponse();
    }
    const auto& msg = cmd.push_session_config();

    sst::session::SessionConfig config;
    config.match_uuid = msg.match_uuid();
    config.user_uuid = msg.user_uuid();
    config.sport = msg.sport();
    config.num_periods = msg.num_periods();
    config.period_length_seconds = msg.period_length_seconds();
    config.rtmp_url = msg.has_rtmp_url() ? msg.rtmp_url() : "";
    config.stream_key = msg.has_stream_key() ? msg.stream_key() : "";
    config.video_output_path = msg.video_output_path();
    config.thumbnail_output_path = msg.thumbnail_output_path();
    config.team_a_id = msg.team_a_id();
    config.team_b_id = msg.team_b_id();
    config.team_a_name = msg.team_a_name();
    config.team_b_name = msg.team_b_name();
    config.team_a_color_hex = msg.team_a_color_hex();
    config.team_b_color_hex = msg.team_b_color_hex();
    config.auto_stop_minutes = msg.has_auto_stop_minutes() ? msg.auto_stop_minutes()
                                                           : sst::session::kDefaultAutoStopMinutes;

    sst_cam::CommandResponse resp;
    const auto prior = session_.Snapshot();
    if (session_.ApplySessionConfig(config)) {
        if (prior.config && prior.config->match_uuid == config.match_uuid &&
            (prior.phase == sst::session::SessionPhase::kRecording || prior.is_streaming)) {
            resp.set_status(sst_cam::OK);
            return resp;
        }
        // Configuring a match clears the overlay: a previous match's scoreboard
        // is removed from the live preview, and the board only reappears at
        // kickoff (no active match -> no overlay).
        controller_.Clear();
        resp.set_status(sst_cam::ResponseStatus::OK);
    } else {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message(
            "session config rejected: WiFi Direct group must be up first (out of order)");
    }
    return resp;
}

auto SessionHandler::SnapshotResponse() const -> sst_cam::CommandResponse {
    const auto state = session_.Snapshot();
    sst_cam::CommandResponse response;
    response.set_status(sst_cam::OK);
    auto* snapshot = response.mutable_session_snapshot();
    switch (state.phase) {
        case sst::session::SessionPhase::kIdle:
        case sst::session::SessionPhase::kConnected:
        case sst::session::SessionPhase::kWifiReady:
            snapshot->set_session_phase(sst_cam::SESSION_IDLE);
            break;
        case sst::session::SessionPhase::kConfigured:
            snapshot->set_session_phase(sst_cam::SESSION_CONFIGURED);
            break;
        case sst::session::SessionPhase::kReady:
            snapshot->set_session_phase(sst_cam::SESSION_READY);
            break;
        case sst::session::SessionPhase::kRecording:
            snapshot->set_session_phase(sst_cam::SESSION_RECORDING);
            break;
        case sst::session::SessionPhase::kFinalizing:
            snapshot->set_session_phase(sst_cam::SESSION_FINALIZING);
            break;
    }
    if (camera_) {
        if (const auto index = camera_()) {
            snapshot->set_active_camera_index(*index);
        }
    }
    if (layout_) {
        if (const auto value = layout_()) {
            snapshot->set_preview_layout(*value);
        }
    }
    snapshot->set_is_recording(state.phase == sst::session::SessionPhase::kRecording);
    snapshot->set_is_streaming(state.is_streaming);
    snapshot->set_wifi_group_up(state.wifi_group_up);
    if (state.recording_elapsed_seconds) {
        snapshot->set_recording_elapsed_seconds(*state.recording_elapsed_seconds);
    }
    if (state.config) {
        FillMatchState(state, snapshot->mutable_match_state());
    }
    if (health_) {
        if (const auto value = health_(0)) {
            snapshot->set_camera0_health(*value);
        }
        if (const auto value = health_(1)) {
            snapshot->set_camera1_health(*value);
        }
    }
    if (!state.config && state.last_session) {
        auto* last = snapshot->mutable_last_session();
        last->set_match_uuid(state.last_session->match_uuid);
        last->set_end_clock_seconds(state.last_session->clock_seconds);
        last->set_file_valid(state.last_session->file_valid);
        switch (state.last_session->reason) {
            case sst::session::SessionEndReason::kAppStop:
                last->set_end_reason(sst_cam::SESSION_END_APP_STOP);
                break;
            case sst::session::SessionEndReason::kAutoStop:
                last->set_end_reason(sst_cam::SESSION_END_AUTO_STOP);
                break;
            case sst::session::SessionEndReason::kCameraFailure:
                last->set_end_reason(sst_cam::SESSION_END_CAMERA_FAILURE);
                break;
            case sst::session::SessionEndReason::kReboot:
                last->set_end_reason(sst_cam::SESSION_END_REBOOT);
                break;
        }
    }
    return response;
}

}  // namespace sst::control
