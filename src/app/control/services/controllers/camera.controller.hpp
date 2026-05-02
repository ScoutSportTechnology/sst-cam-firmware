#pragma once

#include <cstdint>
#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/db/ports/camera-repository.hpp"

namespace sst::control {

// Routes "camera.*" BLE commands. Reads / mutates the camera_config row for
// the controller's user. Calibration (camera_calibration) is a factory
// operation and is intentionally not exposed via BLE.
//
// Verbs:
//
//   get_config   → response payload = JSON with every camera_config field.
//
//   set_config   JSON with any subset of fields. The controller fetches the
//                current row, overlays the provided fields, writes back.
//                Live edits to event_clip_pre/post_seconds take effect on
//                the next event triggered via match.record_event.
class CameraController final : public IController {
   public:
    CameraController(sst::db::ICameraRepository& repo, std::int64_t user_id);

    auto Id() const -> std::string_view override { return "camera"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::db::ICameraRepository& repo_;
    std::int64_t user_id_;
};

}  // namespace sst::control
