#include "app/control/services/handlers/recording.handler.hpp"

namespace sst::control {

RecordingHandler::RecordingHandler(sst::session::ISessionManager& session,
                                   sst::storage::IRecordingService& recording)
    : session_(session), recording_(recording) {}

auto RecordingHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kRecordingControl};
}

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
            if (!recording_.StartRecording(state.config->video_output_path,
                                           state.config->thumbnail_output_path)) {
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
            resp.set_status(sst_cam::ResponseStatus::OK);
            return resp;
        }
        case sst_cam::RECORDING_STOP: {
            const auto result = recording_.Stop();
            session_.OnRecordingStop();
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
