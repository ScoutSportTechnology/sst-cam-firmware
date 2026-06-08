#pragma once

#include <filesystem>

#include "app/control/ports/system-stats.hpp"
#include "domain/control/models/system-stats.hpp"

namespace sst::adapters::control {

// Reads device health from the Linux OS: storage from the configured video
// root (statvfs via std::filesystem), RAM + uptime from sysinfo(2), CPU load
// from /proc/loadavg, and SoC temperature from the thermal sysfs zone. Battery
// is reported as 0 until a battery sensor is wired. The storage/RAM/uptime/CPU
// reads work in the dev container; temp is device-specific and best-effort.
class ProcSystemStats final : public sst::control::ISystemStats {
   public:
    explicit ProcSystemStats(std::filesystem::path storage_root);

    [[nodiscard]] auto Read() const -> sst::control::SystemStats override;

   private:
    std::filesystem::path storage_root_;
};

}  // namespace sst::adapters::control
