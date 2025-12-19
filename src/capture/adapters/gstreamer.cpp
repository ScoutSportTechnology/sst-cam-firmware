#pragma once
#include "gstreamer.hpp"

#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstmemory.h>

#include <cstdint>
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

    // sst_cam_v1 (usb cam), sst_cam_v2 (raspberry pi camera module), sst_cam_v3 (nvidia jetson
    // camera module) specific pipelines

    if (config_data_.device.model->contains("sst_cam_v2")) {
        pipeline_str_ = "sst_cam_v2";
    } else if (config_data_.device.model->contains("sst_cam_v3")) {
        pipeline_str_ = "sst_cam_v3";
    } else {
        pipeline_str_ = "sst_cam_v1";
        // TODO: parametrize format, resolution, and framerate
        pipeline_str_ = "v4l2src device=/dev/video" + std::to_string(camera_index_) +
                        " ! video/x-raw,format=YUY2,width=640,height=480,framerate=30/1 "
                        "! videoconvert ! appsink name=sink sync=false max-buffers=2 drop=true";
    }

    gst_pipeline_ = gst_parse_launch(pipeline_str_.c_str(), &gst_err_);
    if (gst_pipeline_ == nullptr) {
        if (gst_err_ != nullptr) {
            g_printerr("Failed to create pipeline: %s\n", gst_err_->message);
            g_error_free(gst_err_);
            gst_err_ = nullptr;
        }
        return;
    }

    gst_sink_ = gst_bin_get_by_name(GST_BIN(gst_pipeline_), "sink");
    if (gst_sink_ == nullptr) {
        g_printerr("Failed to get appsink from pipeline\n");
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
        return;
    }

    GstAppSink* gst_appsink = GST_APP_SINK(gst_sink_);
    gst_app_sink_set_drop(gst_appsink, 1);
    gst_app_sink_set_max_buffers(gst_appsink, 2);
    gst_app_sink_set_emit_signals(gst_appsink, 0);

    const GstStateChangeReturn ret = gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to set pipeline to PLAYING state\n");
        gst_object_unref(gst_pipeline_);
        gst_sink_ = nullptr;
        gst_pipeline_ = nullptr;
        return;
    }

    is_running_ = true;
};

auto GStreamerAdapter::Stop() -> void {
    if (!is_running_) {
        return;
    }
    if (gst_pipeline_ != nullptr) {
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
    }

    if (gst_sink_ != nullptr) {
        gst_object_unref(gst_sink_);
        gst_sink_ = nullptr;
    }

    is_running_ = false;
};

auto GStreamerAdapter::IsRunning() const -> bool { return is_running_; }

auto GStreamerAdapter::Capture() -> std::optional<Frame> {
    if (!is_running_ || (gst_pipeline_ == nullptr) || (gst_sink_ == nullptr)) {
        return std::nullopt;
    }

    GstAppSink* gst_appsink = GST_APP_SINK(gst_sink_);
    constexpr GstClockTime kTimeout = 50 * GST_MSECOND;
    GstSample* gst_sample = gst_app_sink_try_pull_sample(gst_appsink, kTimeout);
    if (gst_sample == nullptr) {
        g_printerr("Failed to pull sample from appsink\n");
        return std::nullopt;
    }
    auto sample_owner = std::shared_ptr<GstSample>(
        gst_sample, [](GstSample* currentGstSample) { gst_sample_unref(currentGstSample); });
    GstCaps* gst_caps = gst_sample_get_caps(gst_sample);
    GstBuffer* gst_buffer = gst_sample_get_buffer(gst_sample);
    if (gst_caps == nullptr || gst_buffer == nullptr) {
        g_printerr("Failed to get caps or buffer from sample\n");
        return std::nullopt;
    }
    GstVideoInfo gst_video_info;
    if (gst_video_info_from_caps(&gst_video_info, gst_caps) == 0) {
        g_printerr("Failed to get video info from caps\n");
        return std::nullopt;
    }
    GstMapInfo gst_map;
    if (gst_buffer_map(gst_buffer, &gst_map, GST_MAP_READ) == 0) {
        g_printerr("Failed to map buffer\n");
        return std::nullopt;
    }
    auto bytes = std::shared_ptr<std::uint8_t>(new std::uint8_t[gst_map.size],
                                               std::default_delete<std::uint8_t[]>());
    std::memcpy(bytes.get(), gst_map.data, gst_map.size);
    gst_buffer_unmap(gst_buffer, &gst_map);

    Frame frame{};
    frame.frame_id = frame_counter_++;
    frame.captured_at = GetCurrentTimestamp();

    FrameGeometry geometry{};
    geometry.width = GST_VIDEO_INFO_WIDTH(&gst_video_info);
    geometry.height = GST_VIDEO_INFO_HEIGHT(&gst_video_info);
    frame.geometry = geometry;

    FramePlane plane{};
    plane.data = bytes.get();
    plane.size = gst_map.size;
    plane.stride = GST_VIDEO_INFO_PLANE_STRIDE(&gst_video_info, 0);
    frame.planes = {plane};

    frame.owner = bytes;
    // TODO: fill format, memory, and other frame fields based on GStreamer
    frame.format = PixelFormat::UNKNOWN;  // Placeholder
    frame.memory = MemoryType::CPU;       // Placeholder
    // pipeline
    return frame;
}
}  // namespace sst::capture::adapters