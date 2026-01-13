#include "gstreamer.hpp"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstmemory.h>
#include <gst/video/video.h>

#include <memory>
#include <vector>

#include "capture/adapters/gstreamer_runtime.hpp"
#include "common/domain/memory_type.hpp"
#include "common/domain/pixel_format.hpp"
#include "common/utils/timestamp.hpp"
#include "config/domain/calibration_config.hpp"
#include "config/domain/device_model.hpp"

namespace sst::capture::adapters {
using sst::capture::domain::FrameGeometry;
using sst::capture::domain::FramePlane;
using sst::common::domain::MemoryType;
using sst::common::domain::PixelFormat;
using sst::common::utils::GetCurrentTimestamp;
using sst::config::domain::CalibrationCameraData;
using sst::config::domain::CalibrationData;
using sst::config::domain::CalibrationDevicesData;
using sst::config::domain::DeviceModel;
using sst::config::domain::FromStringDeviceModel;
using sst::config::domain::ToStringDeviceModel;

GStreamerAdapter::GStreamerAdapter(const ConfigData& config_data, std::uint16_t camera_index)
    : config_data_(config_data), camera_index_(camera_index) {
    GstInitRuntime();
}

auto GStreamerAdapter::Start() -> void {
    if (is_running_) {
        return;
    }

    CalibrationData calibration = config_data_.calibration;
    if (!calibration.devices) {
        return;
    }
    const CalibrationDevicesData devices = *calibration.devices;
    if (!devices.camera) {
        return;
    }
    const std::vector<CalibrationCameraData>& cameras = *devices.camera;

    const CalibrationCameraData* camera = nullptr;
    for (const auto& cam : cameras) {
        if (cam.id && *cam.id == camera_index_) {
            camera = &cam;
            break;
        }
    }
    if (camera == nullptr) {
        return;
    }

    // TODO: parametrize format, resolution, and framerate
    DeviceModel model = DeviceModel::UNKNOWN;
    if (config_data_.device.model) {
        model = FromStringDeviceModel(*config_data_.device.model);
    }

    const unsigned int width = camera->width.value_or(1920);
    const unsigned int height = camera->height.value_or(1080);
    const unsigned int fps = camera->fps.value_or(60);

    switch (model) {
        case DeviceModel::V1:
            pipeline_str_ = "sst_cam_v1";  // placeholder
            break;
        case DeviceModel::V2:
            pipeline_str_ = "sst_cam_v2";  // placeholder
            break;
        case DeviceModel::UNKNOWN:
        default:
            pipeline_str_ = "mfvideosrc device-index=" + std::to_string(camera_index_) +
                            " ! video/x-raw,width=" + std::to_string(width) +
                            ",height=" + std::to_string(height) +
                            ",framerate=" + std::to_string(fps) +
                            "/1"
                            " ! videoconvert"
                            " ! video/x-raw,format=NV12 "
                            " ! tee name=t "
                            " t. ! queue "
                            "     ! appsink name=sink sync=false "
                            " t. ! queue "
                            "     ! autovideosink sync=false";
            break;
    }

    gst_err_ = nullptr;
    gst_pipeline_ = gst_parse_launch(pipeline_str_.c_str(), &gst_err_);
    if (gst_pipeline_ == nullptr) {
        if (gst_err_ != nullptr) {
            g_printerr("Failed to create pipeline: %s\n", gst_err_->message);
            g_error_free(gst_err_);
            gst_err_ = nullptr;
        }
        return;
    }

    gst_bus_ = gst_element_get_bus(gst_pipeline_);
    if (gst_bus_ == nullptr) {
        g_printerr("Failed to get bus from pipeline\n");
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
        return;
    }

    gst_sink_ = gst_bin_get_by_name(GST_BIN(gst_pipeline_), "sink");
    if (gst_sink_ == nullptr) {
        g_printerr("Failed to get appsink from pipeline\n");
        gst_object_unref(gst_bus_);
        gst_bus_ = nullptr;
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
        return;
    }

    GstAppSink* gst_appsink = GST_APP_SINK(gst_sink_);
    gst_app_sink_set_drop(gst_appsink, 1);
    gst_app_sink_set_max_buffers(gst_appsink, 5);
    gst_app_sink_set_emit_signals(gst_appsink, 0);

    const GstStateChangeReturn ret = gst_element_set_state(gst_pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to set pipeline to PLAYING state\n");
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);

        if (gst_sink_ != nullptr) {
            gst_object_unref(gst_sink_);
            gst_sink_ = nullptr;
        }
        if (gst_bus_ != nullptr) {
            gst_object_unref(gst_bus_);
            gst_bus_ = nullptr;
        }
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
        return;
    }

    is_running_ = true;

    // Prime one frame so Capture() can be non-blocking and still return a buffered frame
    {
        constexpr GstClockTime kPrimeTimeout = 2 * GST_SECOND;

        GstSample* gstSample = gst_app_sink_try_pull_sample(gst_appsink, kPrimeTimeout);
        if (gstSample != nullptr) {
            auto owner = std::shared_ptr<GstSample>(
                gstSample, [](GstSample* sample) { gst_sample_unref(sample); });

            auto capturedFrame = CreateFrameFromSample(owner.get());
            if (capturedFrame) {
                std::lock_guard<std::mutex> lastFrameLock(last_frame_mtx_);
                last_frame_ = *capturedFrame;
            }
        } else {
            spdlog::warn("GStreamer: did not receive initial frame during Start() prime");
        }
    }
}

auto GStreamerAdapter::Stop() -> void {
    if (gst_pipeline_ != nullptr) {
        gst_element_set_state(gst_pipeline_, GST_STATE_NULL);
    }

    if (gst_sink_ != nullptr) {
        gst_object_unref(gst_sink_);
        gst_sink_ = nullptr;
    }

    if (gst_bus_ != nullptr) {
        gst_object_unref(gst_bus_);
        gst_bus_ = nullptr;
    }

    if (gst_pipeline_ != nullptr) {
        gst_object_unref(gst_pipeline_);
        gst_pipeline_ = nullptr;
    }

    if (gst_err_ != nullptr) {
        g_error_free(gst_err_);
        gst_err_ = nullptr;
    }

    std::lock_guard<std::mutex> lastFrameLock(last_frame_mtx_);
    last_frame_.reset();

    is_running_ = false;
};

auto GStreamerAdapter::IsRunning() const -> bool {
    return is_running_ && (gst_pipeline_ != nullptr);
}

auto GStreamerAdapter::HandleBusMessages() -> bool {
    if (gst_bus_ == nullptr) {
        return false;
    }
    while (GstMessage* msg = gst_bus_pop_filtered(
               gst_bus_, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* gerr = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &gerr, &dbg);

            g_printerr("GStreamer ERROR: %s\n", ((gerr != nullptr) && (gerr->message != nullptr))
                                                    ? gerr->message
                                                    : "(null)");
            if (dbg != nullptr) {
                g_printerr("debug: %s\n", dbg);
            }

            if (gerr != nullptr) {
                g_error_free(gerr);
            }
            if (dbg != nullptr) {
                g_free(dbg);
            }

            gst_message_unref(msg);
            Stop();
            return false;
        }

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            gst_message_unref(msg);
            Stop();
            return false;
        }

        gst_message_unref(msg);
    }
    return true;
}

auto GStreamerAdapter::CreateFrameFromSample(GstSample* gst_sample) -> std::optional<Frame> {
    if (gst_sample == nullptr) {
        return std::nullopt;
    }

    GstCaps* caps = gst_sample_get_caps(gst_sample);
    GstBuffer* buf = gst_sample_get_buffer(gst_sample);
    if (caps == nullptr || buf == nullptr) {
        return std::nullopt;
    }

    GstVideoInfo info;
    if (gst_video_info_from_caps(&info, caps) == 0) {
        return std::nullopt;
    }

    GstBuffer* buf_ref = gst_buffer_ref(buf);

    auto map = std::make_shared<GstMapInfo>();
    if (gst_buffer_map(buf_ref, map.get(), GST_MAP_READ) == 0) {
        gst_buffer_unref(buf_ref);
        return std::nullopt;
    }

    Frame frame{};
    frame.frame_id = frame_counter_++;
    frame.captured_at = GetCurrentTimestamp();

    frame.geometry = FrameGeometry{
        .width = static_cast<std::uint32_t>(GST_VIDEO_INFO_WIDTH(&info)),
        .height = static_cast<std::uint32_t>(GST_VIDEO_INFO_HEIGHT(&info)),
    };

    frame.owner = std::shared_ptr<void>(buf_ref, [map](void* gstreamerPipeline) {
        auto* gstreamerBuffer = static_cast<GstBuffer*>(gstreamerPipeline);
        gst_buffer_unmap(gstreamerBuffer, map.get());
        gst_buffer_unref(gstreamerBuffer);
    });

    const guint number_of_planes = GST_VIDEO_INFO_N_PLANES(&info);
    frame.planes.clear();
    frame.planes.reserve(number_of_planes);
    for (guint i = 0; i < number_of_planes; ++i) {
        const guint plane_stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, i);
        const gsize plane_offset = GST_VIDEO_INFO_PLANE_OFFSET(&info, i);

        gsize next_plane_offset = map->size;
        if (i + 1 < number_of_planes) {
            next_plane_offset = GST_VIDEO_INFO_PLANE_OFFSET(&info, i + 1);
        }
        if(plane_offset >= next_plane_offset || next_plane_offset > map->size || next_plane_offset < plane_offset) {
            return std::nullopt;
        }

        FramePlane plane{};
        plane.data = map->data + plane_offset;
        plane.size = next_plane_offset - plane_offset;
        plane.stride = plane_stride;
        frame.planes.push_back(plane);
    }

    const GstVideoFormat fmt = GST_VIDEO_INFO_FORMAT(&info);
    switch (fmt) {
        case GST_VIDEO_FORMAT_BGR:
            frame.format = PixelFormat::BGR8;
            break;
        case GST_VIDEO_FORMAT_BGRA:
            frame.format = PixelFormat::BGRA8;
            break;
        case GST_VIDEO_FORMAT_NV12:
            frame.format = PixelFormat::NV12;
            break;
        case GST_VIDEO_FORMAT_I420:
            frame.format = PixelFormat::I420;
            break;
        case GST_VIDEO_FORMAT_YUY2:
            frame.format = PixelFormat::YUYV;
            break;
        case GST_VIDEO_FORMAT_RGB:
            frame.format = PixelFormat::RGB8;
            break;
        case GST_VIDEO_FORMAT_RGBA:
            frame.format = PixelFormat::RGBA8;
            break;
        case GST_VIDEO_FORMAT_GRAY8:
            frame.format = PixelFormat::GRAY8;
            break;
        default:
            frame.format = PixelFormat::UNKNOWN;
    }

    frame.memory = MemoryType::CPU;
    return frame;
}

auto GStreamerAdapter::Capture() -> std::optional<Frame> {
    if (!is_running_ || (gst_pipeline_ == nullptr) || (gst_sink_ == nullptr) ||
        (gst_bus_ == nullptr)) {
        return std::nullopt;
    }
    if (!HandleBusMessages()) {
        return std::nullopt;
    }

    GstAppSink* appsink = GST_APP_SINK(gst_sink_);

    GstSample* captureGstSample = nullptr;
    GstSample* last = nullptr;

    while ((captureGstSample = gst_app_sink_try_pull_sample(appsink, 0)) != nullptr) {
        if (last != nullptr) {
            gst_sample_unref(last);
        }
        last = captureGstSample;
    }

    if (last == nullptr) {
        std::lock_guard<std::mutex> lastFrameLock(last_frame_mtx_);
        return last_frame_ ? std::optional<Frame>(*last_frame_) : std::nullopt;
    }

    auto owner =
        std::shared_ptr<GstSample>(last, [](GstSample* gstSample) { gst_sample_unref(gstSample); });
    auto fresh = CreateFrameFromSample(owner.get());
    if (!fresh) {
        std::lock_guard<std::mutex> lastFrameLock(last_frame_mtx_);
        return last_frame_ ? std::optional<Frame>(*last_frame_) : std::nullopt;
    }

    {
        std::lock_guard<std::mutex> lastFrameLock(last_frame_mtx_);
        last_frame_ = *fresh;
    }
    return fresh;
}

}  // namespace sst::capture::adapters