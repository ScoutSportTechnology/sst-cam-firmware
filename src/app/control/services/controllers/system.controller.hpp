#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "domain/config/models/device.hpp"

namespace sst::control {

// Routes "system.*" BLE commands. Read-only inspection of static device info
// (loaded from device.json at boot) plus the device clock.
//
// Verbs:
//
//   version  → response payload = the device firmware version string
//              (cfg.device.version, or empty if unset).
//   info     → response payload = JSON with name / model / version /
//              manufacturer / serial_number / timezone.
//   time     → response payload = current device time as ISO 8601 UTC
//              ("YYYY-MM-DDTHH:MM:SS"). Companion app uses this to
//              cross-check its own clock; setting the device clock requires
//              root privileges and is intentionally not exposed via BLE.
class SystemController final : public IController {
   public:
    explicit SystemController(const sst::config::DeviceData& device);

    auto Id() const -> std::string_view override { return "system"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    const sst::config::DeviceData& device_;
};

}  // namespace sst::control
