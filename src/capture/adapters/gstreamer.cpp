#include "gstreamer.hpp"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstring>
#include <memory>

#include "common/utils/timestamp.hpp"
#include "config/domain/device_model.hpp"

namespace sst::capture::adapters {
using sst::common::utils::GetCurrentTimestamp;
using sst::config::domain::DeviceModel;

auto GStreamerAdapter::Start() -> void {
    if (is_running_) {
        return;
    }

    gst_init(nullptr, nullptr);

    if (config_data_.device.model->contains("sst_cam_v1")) {
      pipeline_ = "v4l2src device=/dev/video" + std::to_string(camera_index_) +
                  " ! video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 "
                  "! videoconvert ! appsink name=sink";

    } else if (config_data_.device.model->contains("sst_cam_v2")) {
    } else if (config_data_.device.model->contains("sst_cam_v3")) {
    } else {
        // Unknown device model
        return;
    }

    // TODO: Initialize GStreamer pipeline and start capturing
    is_running_ = true;
};

auto GStreamerAdapter::Stop() -> void {
    if (!is_running_) {
        return;
    }

    // TODO: Stop GStreamer pipeline and release resources
    is_running_ = false;
};

auto GStreamerAdapter::IsRunning() const -> bool { return is_running_; }

auto GStreamerAdapter::Capture() -> std::optional<Frame> {
    if (!is_running_) {
        return std::nullopt;
    }

    Frame frame{};
    frame.frame_id = frame_counter_++;
    // TODO: fill captured_at, format, geometry, planes, owner, with actual data from GStreamer
    // pipeline
    return frame;
}
}  // namespace sst::capture::adapters