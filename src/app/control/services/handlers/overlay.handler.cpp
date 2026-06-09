#include "app/control/services/handlers/overlay.handler.hpp"

#include <utility>

#include "app/overlay/binding-sync.hpp"
#include "app/overlay/overlay-proto-mapper.hpp"

namespace sst::control {

OverlayHandler::OverlayHandler(sst::session::ISessionManager& session,
                               sst::overlay::OverlayController& controller, Clock now_ms)
    : session_(session), controller_(controller), now_ms_(std::move(now_ms)) {}

auto OverlayHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kPushOverlayLayout};
}

auto OverlayHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;

    if (!session_.OnOverlayConfigured()) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message(
            "overlay layout rejected: a session config must be applied first (out of order)");
        return resp;
    }

    controller_.SetLayout(sst::overlay::MapLayoutFromProto(cmd.push_overlay_layout().layout()));
    controller_.SetBindingData(sst::overlay::BuildBindingData(session_.Snapshot()));
    controller_.Refresh(now_ms_ ? now_ms_() : 0);

    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

}  // namespace sst::control
