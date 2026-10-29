#include "app/control/services/handlers/recording.handler.hpp"

#include <utility>

#include "app/control/services/handlers/quality-mapping.hpp"
#include "domain/session/models/session-phase.hpp"

namespace sst::control {

RecordingHandler::RecordingHandler(sst::session::ISessionManager& session,
                                   sst::storage::IRecordingService& recording,
                                   sst::overlay::IOverlayTimelineRecorder& timeline,
                                   sst::raw_capture::ProxyOwners& proxy, Clock now_ms,
                                   std::function<sst_cam::CameraHealth(std::uint32_t)> health)
    : session_(session),
      recording_(recording),
      timeline_(timeline),
      proxy_(proxy),
      now_ms_(std::move(now_ms)),
      health_(std::move(health)) {}

auto RecordingHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kRecordingControl};
}

// floor-ok: flat linear command dispatch (start/stop/status guards); splitting it
// would scatter the record lifecycle across helpers without reducing real branching.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto RecordingHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;
    const auto action = cmd.recording_control().action();

    switch (action) {
        case sst_cam::RECORDING_START: {
            const auto state = session_.Snapshot();
            if (!state.config) {
                resp.set_status(sst_cam::ResponseStatus::ERROR);
                resp.set_error_message("recording start with no active session config");
                return resp;
            }
            // Gate on phase BEFORE touching the recorder — otherwise a start in a
            // non-Ready phase spins up NVENC and writes a stray MP4/thumbnail that
            // the SM-rejection rollback below then has to finalize and orphan.
            if (state.phase != sst::session::SessionPhase::kReady) {
                resp.set_status(sst_cam::ResponseStatus::ERROR);
                resp.set_error_message("recording start rejected: session not ready");
                return resp;
            }
            if (health_ && (health_(0) != sst_cam::CAMERA_HEALTH_OK ||
                            health_(1) != sst_cam::CAMERA_HEALTH_OK)) {
                resp.set_status(sst_cam::DEVICE_INOPERABLE);
                return resp;
            }
            const auto quality = ResolveQuality(cmd.recording_control().has_quality(),
                                                cmd.recording_control().quality());
            if (!recording_.StartRecording(state.config->video_output_path,
                                           state.config->thumbnail_output_path, quality)) {
                resp.set_status(sst_cam::ResponseStatus::ERROR);
                resp.set_error_message("recorder failed to start (disk full or already running)");
                return resp;
            }
            if (!session_.OnRecordingStart()) {
                // Roll back if the SM rejects (not in Ready) — keep state consistent.
                recording_.Stop();
                resp.set_status(sst_cam::ResponseStatus::ERROR);
                resp.set_error_message("recording start rejected: session not ready");
                return resp;
            }
            (void)proxy_.Start(sst::raw_capture::ProxyOwners::Owner::kRecording,
                               state.config->match_uuid, state.config->video_output_path);
            // Begin capturing the overlay timeline beside the L1 MP4 (#6 F6b),
            // anchored at the overlay clock so the burn (F6c) can realign it.
            timeline_.Start(state.config->video_output_path, now_ms_ ? now_ms_() : 0);
            resp.set_status(sst_cam::ResponseStatus::OK);
            return resp;
        }
        case sst_cam::RECORDING_STOP: {
            if (session_.Phase() != sst::session::SessionPhase::kRecording) {
                resp.set_status(sst_cam::ERROR);
                resp.set_error_message("no active recording to stop");
                return resp;
            }
            const auto result = recording_.Stop();
            proxy_.Stop(sst::raw_capture::ProxyOwners::Owner::kRecording);
            // Flush the overlay timeline next to the L1 MP4 (#6 F6b).
            timeline_.Stop();
            session_.OnRecordingStop(result.success);
            resp.set_status(result.success ? sst_cam::ResponseStatus::OK
                                           : sst_cam::ResponseStatus::ERROR);
            if (!result.success) {
                resp.set_error_message("no active recording to stop");
            }
            return resp;
        }
        case sst_cam::RECORDING_PAUSE:
            resp.set_status(recording_.Pause() ? sst_cam::ResponseStatus::OK
                                               : sst_cam::ResponseStatus::ERROR);
            if (resp.status() != sst_cam::ResponseStatus::OK) {
                resp.set_error_message("cannot pause: not recording");
            }
            return resp;
        case sst_cam::RECORDING_RESUME:
            resp.set_status(recording_.Resume() ? sst_cam::ResponseStatus::OK
                                                : sst_cam::ResponseStatus::ERROR);
            if (resp.status() != sst_cam::ResponseStatus::OK) {
                resp.set_error_message("cannot resume: not paused");
            }
            return resp;
        default:
            resp.set_status(sst_cam::ResponseStatus::ERROR);
            resp.set_error_message("unknown recording action");
            return resp;
    }
}

}  // namespace sst::control
