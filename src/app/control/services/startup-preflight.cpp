#include "app/control/services/startup-preflight.hpp"

#include <utility>

namespace sst::control {

StartupPreflight::StartupPreflight(std::function<StartupProbes()> probe, StartupActions actions)
    : probe_(std::move(probe)), actions_(std::move(actions)) {}

auto StartupPreflight::Poll() -> void {
    std::lock_guard lock(mutex_);
    latest_ = probe_();
    polled_ = true;
    if (!ble_up_ && latest_.bluetooth == sst_cam::COMPONENT_STATUS_OK) {
        ble_up_ = actions_.ble();
    }
    if (!network_up_ && (latest_.wifi != sst_cam::COMPONENT_STATUS_MISSING ||
                         latest_.ethernet != sst_cam::COMPONENT_STATUS_MISSING)) {
        network_up_ = actions_.network();
    }
    if (network_up_ && !http_up_) {
        http_up_ = actions_.http();
    }
    if (!pipeline_up_ && latest_.gpu == sst_cam::COMPONENT_STATUS_OK &&
        latest_.camera0 == sst_cam::CAMERA_HEALTH_OK &&
        latest_.camera1 == sst_cam::CAMERA_HEALTH_OK) {
        pipeline_up_ = actions_.pipeline();
        if (!pipeline_up_) {
            latest_.camera0 = sst_cam::CAMERA_HEALTH_DOWN;
            latest_.camera1 = sst_cam::CAMERA_HEALTH_DOWN;
        }
    }
}

auto StartupPreflight::Ready() const -> bool {
    std::lock_guard lock(mutex_);
    return polled_;
}

auto StartupPreflight::TakeReadyNotification() -> bool {
    std::lock_guard lock(mutex_);
    if (!polled_ || ready_notified_) {
        return false;
    }
    ready_notified_ = true;
    return true;
}

auto StartupPreflight::Components() const -> sst_cam::DeviceComponents {
    std::lock_guard lock(mutex_);
    sst_cam::DeviceComponents result;
    result.set_gpu(latest_.gpu);
    result.set_wifi(latest_.wifi);
    result.set_ethernet(latest_.ethernet);
    result.set_bluetooth(latest_.bluetooth);
    result.set_storage(latest_.storage);
    result.set_clock_synced(latest_.clock_synced);
    return result;
}

auto StartupPreflight::CameraHealth(std::uint32_t index) const -> sst_cam::CameraHealth {
    std::lock_guard lock(mutex_);
    if (index == 0) {
        return latest_.camera0;
    }
    return index == 1 ? latest_.camera1 : sst_cam::CAMERA_HEALTH_UNKNOWN;
}

}  // namespace sst::control
