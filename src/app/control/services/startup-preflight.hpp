#pragma once

#include <cstdint>
#include <functional>
#include <mutex>

#include "bluetooth.pb.h"

namespace sst::control {

struct StartupProbes {
    sst_cam::CameraHealth camera0;
    sst_cam::CameraHealth camera1;
    sst_cam::ComponentStatus gpu;
    sst_cam::ComponentStatus wifi;
    sst_cam::ComponentStatus ethernet;
    sst_cam::ComponentStatus bluetooth;
    sst_cam::ComponentStatus storage;
    bool clock_synced;
};

struct StartupActions {
    std::function<bool()> ble;
    std::function<bool()> network;
    std::function<bool()> http;
    std::function<bool()> pipeline;
};

class StartupPreflight {
   public:
    StartupPreflight(std::function<StartupProbes()> probe, StartupActions actions);
    auto Poll() -> void;
    [[nodiscard]] auto Ready() const -> bool;
    auto TakeReadyNotification() -> bool;
    [[nodiscard]] auto Components() const -> sst_cam::DeviceComponents;
    [[nodiscard]] auto CameraHealth(std::uint32_t index) const -> sst_cam::CameraHealth;

   private:
    std::function<StartupProbes()> probe_;
    StartupActions actions_;
    mutable std::mutex mutex_;
    StartupProbes latest_{};
    bool ble_up_{false};
    bool network_up_{false};
    bool http_up_{false};
    bool pipeline_up_{false};
    bool polled_{false};
    bool ready_notified_{false};
};

}  // namespace sst::control
