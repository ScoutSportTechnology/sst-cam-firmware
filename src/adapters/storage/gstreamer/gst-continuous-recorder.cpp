#include "adapters/storage/gstreamer/gst-continuous-recorder.hpp"

#include <cstring>

#include <fmt/format.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <spdlog/spdlog.h>

#include "domain/common/models/pixel-format.hpp"

namespace sst::adapters::storage {

namespace {

constexpr const char* kAppsrcName = "src";
constexpr const char* kEncoderName = "enc";
constexpr int kFramerate = 30;

auto GstFormatFor(sst::common::PixelFormat fmt) -> const char* {
    switch (fmt) {
        case sst::common::PixelFormat::BGR8: return "BGR";
        case sst::common::PixelFormat::RGB8: return "RGB";
        case sst::common::PixelFormat::BGRA8: return "BGRA";
        case sst::common::PixelFormat::RGBA8: return "RGBA";
        case sst::common::PixelFormat::GRAY8: return "GRAY8";
        case sst::common::PixelFormat::NV12: return "NV12";
        case sst::common::PixelFormat::I420: return "I420";
        case sst::common::PixelFormat::YUYV: return "YUY2";
    }
    return "BGR";
}

}  // namespace

GstContinuousRecorder::GstContinuousRecorder() { gst_init(nullptr, nullptr); }

GstContinuousRecorder::~GstContinuousRecorder() {
    if (running_) {
        (void)Stop();
    }
}

auto GstContinuousRecorder::Start(const std::filesystem::path& output_mp4) -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        return false;
    }

    // NVENC encode chain. nvvidconv + nvv4l2h264enc exist only on the Jetson, so
    // this launch fails in the dev container — the recorder is hardware-bound.
    const std::string desc = fmt::format(
        "appsrc name={src} is-live=true format=time do-timestamp=true ! "
        "videoconvert ! nvvidconv ! nvv4l2h264enc name={enc} insert-sps-pps=true ! "
        "h264parse ! mp4mux ! filesink location={loc}",
        fmt::arg("src", kAppsrcName), fmt::arg("enc", kEncoderName),
        fmt::arg("loc", output_mp4.string()));

    GError* err = nullptr;
    pipeline_ = gst_parse_launch(desc.c_str(), &err);
    if (pipeline_ == nullptr || err != nullptr) {
        spdlog::error("GstContinuousRecorder: gst_parse_launch failed: {}",
                      err != nullptr ? err->message : "unknown");
        if (err != nullptr) {
            g_error_free(err);
        }
        Teardown();
        return false;
    }

    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), kAppsrcName);
    encoder_ = gst_bin_get_by_name(GST_BIN(pipeline_), kEncoderName);
    if (appsrc_ == nullptr) {
        spdlog::error("GstContinuousRecorder: appsrc not found");
        Teardown();
        return false;
    }

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstContinuousRecorder: failed to set PLAYING");
        Teardown();
        return false;
    }

    caps_set_ = false;
    paused_ = false;
    running_ = true;
    spdlog::info("GstContinuousRecorder: started -> {}", output_mp4.string());
    return true;
}

auto GstContinuousRecorder::Pause() -> void { paused_ = true; }

auto GstContinuousRecorder::Resume() -> void {
    if (!running_) {
        return;
    }
    // Force a fresh IDR so the file resumes cleanly into the same muxer.
    if (encoder_ != nullptr) {
        GstPad* sink = gst_element_get_static_pad(encoder_, "sink");
        if (sink != nullptr) {
            gst_pad_send_event(sink, gst_video_event_new_downstream_force_key_unit(
                                         GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                         GST_CLOCK_TIME_NONE, TRUE, 1));
            gst_object_unref(sink);
        }
    }
    paused_ = false;
}

auto GstContinuousRecorder::Stop() -> bool {
    std::lock_guard lock(mtx_);
    if (!running_) {
        return false;
    }
    if (appsrc_ != nullptr) {
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));
    }
    // Wait for EOS (or error) so mp4mux writes the moov atom and the file is
    // playable.
    if (pipeline_ != nullptr) {
        GstBus* bus = gst_element_get_bus(pipeline_);
        if (bus != nullptr) {
            GstMessage* msg = gst_bus_timed_pop_filtered(
                bus, 5 * GST_SECOND,
                static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (msg != nullptr) {
                gst_message_unref(msg);
            }
            gst_object_unref(bus);
        }
    }
    Teardown();
    spdlog::info("GstContinuousRecorder: stopped + finalized");
    return true;
}

auto GstContinuousRecorder::IsRunning() const -> bool { return running_; }

auto GstContinuousRecorder::Push(const sst::capture::Frame& frame) -> void {
    if (!running_ || paused_ || appsrc_ == nullptr || frame.planes.empty()) {
        return;
    }
    std::lock_guard lock(mtx_);
    if (!caps_set_) {
        GstCaps* caps = gst_caps_new_simple(
            "video/x-raw", "format", G_TYPE_STRING, GstFormatFor(frame.format), "width",
            G_TYPE_INT, static_cast<int>(frame.geometry.width), "height", G_TYPE_INT,
            static_cast<int>(frame.geometry.height), "framerate", GST_TYPE_FRACTION, kFramerate, 1,
            nullptr);
        gst_app_src_set_caps(GST_APP_SRC(appsrc_), caps);
        gst_caps_unref(caps);
        caps_set_ = true;
    }

    const auto& plane = frame.planes[0];
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, plane.size, nullptr);
    if (buffer == nullptr) {
        return;
    }
    gst_buffer_fill(buffer, 0, plane.data, plane.size);
    if (gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer) != GST_FLOW_OK) {
        spdlog::warn("GstContinuousRecorder::Push: push_buffer failed");
    }
}

auto GstContinuousRecorder::Teardown() -> void {
    if (encoder_ != nullptr) {
        gst_object_unref(encoder_);
        encoder_ = nullptr;
    }
    if (appsrc_ != nullptr) {
        gst_object_unref(appsrc_);
        appsrc_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    running_ = false;
    paused_ = false;
    caps_set_ = false;
}

}  // namespace sst::adapters::storage
