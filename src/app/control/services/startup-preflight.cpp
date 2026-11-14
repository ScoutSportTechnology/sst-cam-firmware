#include "app/control/services/startup-preflight.hpp"

#include <condition_variable>
#include <utility>

namespace sst::control {

StartupPreflight::StartupPreflight(std::function<StartupProbes()> probe, StartupActions actions)
    : probe_(std::move(probe)), actions_(std::move(actions)) {}

auto StartupPreflight::Poll() -> void {
    std::lock_guard poll_lock(poll_mutex_);
    auto latest = probe_();
    latest.bluetooth =
        actions_.ble() ? sst_cam::COMPONENT_STATUS_OK : sst_cam::COMPONENT_STATUS_DEGRADED;
    if (!network_up_) {
        network_up_ = actions_.network();
    }
    if (network_up_ && !http_up_) {
        http_up_ = actions_.http();
    }
    if (!pipeline_up_ && latest.gpu == sst_cam::COMPONENT_STATUS_OK &&
        latest.camera0 == sst_cam::CAMERA_HEALTH_OK &&
        latest.camera1 == sst_cam::CAMERA_HEALTH_OK) {
        pipeline_up_ = actions_.pipeline();
        if (!pipeline_up_) {
            latest.camera0 = sst_cam::CAMERA_HEALTH_DOWN;
            latest.camera1 = sst_cam::CAMERA_HEALTH_DOWN;
        }
    }
    std::lock_guard lock(mutex_);
    latest_ = latest;
    polled_ = true;
}

auto StartupPreflight::RunPeriodically(const std::stop_token& stop,
                                       std::chrono::milliseconds interval,
                                       const std::function<void()>& after_poll) -> void {
    std::condition_variable_any wake;
    std::mutex wait_mutex;
    while (!stop.stop_requested()) {
        {
            std::unique_lock wait_lock(wait_mutex);
            wake.wait_for(wait_lock, stop, interval, [] { return false; });
        }
        if (stop.stop_requested()) {
            break;
        }
        Poll();
        after_poll();
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
