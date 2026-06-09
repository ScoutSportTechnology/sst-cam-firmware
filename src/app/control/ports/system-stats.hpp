#pragma once

#include "domain/control/models/system-stats.hpp"

namespace sst::control {

// Source of device health metrics for telemetry. Implemented by an adapter that
// reads the OS / hardware (storage, /proc, sysfs); faked in tests.
class ISystemStats {
   public:
    virtual ~ISystemStats() = default;

    [[nodiscard]] virtual auto Read() const -> SystemStats = 0;
};

}  // namespace sst::control
