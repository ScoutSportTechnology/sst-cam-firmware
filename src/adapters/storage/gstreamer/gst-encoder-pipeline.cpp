#include "adapters/storage/gstreamer/gst-encoder-pipeline.hpp"

#include <cstring>
#include <utility>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sst::adapters::storage {

namespace {

constexpr const char* kAppsrcName = "src";
constexpr const char* kSegmentSinkName = "segment_sink";
constexpr const char* kRingSinkName = "ring_sink";

// Compute total byte size from a Frame's planes.
auto FrameByteSize(const sst::capture::Frame& frame) -> std::size_t {
    std::size_t total = 0;
    for (const auto& plane : frame.planes) {
        total += plane.size;
    }
    return total;
}

}  // namespace

GstEncoderPipeline::GstEncoderPipeline(Config config) : config_(std::move(config)) {
    gst_init(nullptr, nullptr);
}

GstEncoderPipeline::~GstEncoderPipeline() {
    if (running_) {
        Stop();
    }
}

auto GstEncoderPipeline::SetSegmentSink(EncodedSink sink) -> void {
    std::lock_guard lock(mtx_);
    segment_sink_ = std::move(sink);
}

auto GstEncoderPipeline::SetRingSink(EncodedSink sink) -> void {
    std::lock_guard lock(mtx_);
    ring_sink_ = std::move(sink);
}

auto GstEncoderPipeline::Start() -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        return true;
    }
    if (!BuildPipeline()) {
        Teardown();
        return false;
    }

    const auto ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstEncoderPipeline: failed to set PLAYING");
        Teardown();
        return false;
    }
    running_ = true;
    pushed_frames_ = 0;
    spdlog::info("GstEncoderPipeline: started ({}x{}@{} fps, bitrate={} kbps)", config_.width,
                 config_.height, config_.framerate, config_.bitrate_bps / 1000);
    return true;
}

auto GstEncoderPipeline::Stop() -> void {
    std::lock_guard lock(mtx_);
    if (!running_) {
        return;
    }
    if (appsrc_ != nullptr) {
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));
    }
    if (pipeline_ != nullptr) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    Teardown();
    running_ = false;
    spdlog::info("GstEncoderPipeline: stopped after {} frames", pushed_frames_);
}

auto GstEncoderPipeline::IsRunning() const -> bool { return running_; }

auto GstEncoderPipeline::BuildPipeline() -> bool {
    const std::string launch = fmt::format(
        "appsrc name={src} is-live=true format=time do-timestamp=true "
        "  caps=\"video/x-raw,format={fmt},width={w},height={h},framerate={fps}/1\""
        " ! videoconvert ! nvvidconv "
        " ! \"video/x-raw(memory:NVMM),format=NV12,width={w},height={h},framerate={fps}/1\""
        " ! nvv4l2h264enc bitrate={br} insert-sps-pps=true iframeinterval={iv} "
        "                 maxperf-enable=true "
        " ! h264parse config-interval=1 "
        " ! \"video/x-h264,stream-format=byte-stream,alignment=au\" "
        " ! tee name=t "
        " t. ! queue ! appsink name={seg} emit-signals=true sync=false drop=false "
        " t. ! queue ! appsink name={ring} emit-signals=true sync=false drop=false ",
        fmt::arg("src", kAppsrcName), fmt::arg("seg", kSegmentSinkName),
        fmt::arg("ring", kRingSinkName), fmt::arg("fmt", config_.raw_format),
        fmt::arg("w", config_.width), fmt::arg("h", config_.height),
        fmt::arg("fps", config_.framerate), fmt::arg("br", config_.bitrate_bps),
        fmt::arg("iv", config_.iframe_interval_frames));

    GError* err = nullptr;
    pipeline_ = gst_parse_launch(launch.c_str(), &err);
    if (err != nullptr) {
        spdlog::error("GstEncoderPipeline: gst_parse_launch failed: {}", err->message);
        g_error_free(err);
        return false;
    }
    if (pipeline_ == nullptr) {
        return false;
    }

    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), kAppsrcName);
    segment_appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), kSegmentSinkName);
    ring_appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), kRingSinkName);
    if (appsrc_ == nullptr || segment_appsink_ == nullptr || ring_appsink_ == nullptr) {
        spdlog::error("GstEncoderPipeline: pipeline missing named elements");
        return false;
    }

    g_signal_connect(segment_appsink_, "new-sample",
                     G_CALLBACK(&GstEncoderPipeline::OnSegmentSampleStatic), this);
    g_signal_connect(ring_appsink_, "new-sample",
                     G_CALLBACK(&GstEncoderPipeline::OnRingSampleStatic), this);
    return true;
}

auto GstEncoderPipeline::Teardown() -> void {
    if (segment_appsink_ != nullptr) {
        gst_object_unref(segment_appsink_);
        segment_appsink_ = nullptr;
    }
    if (ring_appsink_ != nullptr) {
        gst_object_unref(ring_appsink_);
        ring_appsink_ = nullptr;
    }
    if (appsrc_ != nullptr) {
        gst_object_unref(appsrc_);
        appsrc_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}

auto GstEncoderPipeline::Push(const sst::capture::Frame& frame) -> void {
    if (!running_ || appsrc_ == nullptr) {
        return;
    }
    const auto total = FrameByteSize(frame);
    if (total == 0) {
        return;
    }

    auto* gst_buf = gst_buffer_new_allocate(nullptr, total, nullptr);
    if (gst_buf == nullptr) {
        spdlog::warn("GstEncoderPipeline::Push: gst_buffer_new_allocate failed (size={})", total);
        return;
    }

    GstMapInfo map{};
    if (gst_buffer_map(gst_buf, &map, GST_MAP_WRITE) == 0) {
        gst_buffer_unref(gst_buf);
        return;
    }
    std::size_t offset = 0;
    for (const auto& plane : frame.planes) {
        if (plane.data != nullptr && plane.size > 0) {
            std::memcpy(map.data + offset, plane.data, plane.size);
            offset += plane.size;
        }
    }
    gst_buffer_unmap(gst_buf, &map);

    GST_BUFFER_PTS(gst_buf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(gst_buf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(gst_buf) = GST_CLOCK_TIME_NONE;

    const auto ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), gst_buf);
    if (ret != GST_FLOW_OK) {
        spdlog::warn("GstEncoderPipeline::Push: gst_app_src_push_buffer={}",
                     gst_flow_get_name(ret));
        return;
    }
    ++pushed_frames_;
}

auto GstEncoderPipeline::OnEncodedSample(GstAppSink* sink, EncodedSink& target) -> GstFlowReturn {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (sample == nullptr) {
        return GST_FLOW_OK;
    }

    GstBuffer* buf = gst_sample_get_buffer(sample);
    if (buf == nullptr) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstMapInfo map{};
    if (gst_buffer_map(buf, &map, GST_MAP_READ) == 0) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    sst::storage::EncodedNal nal;
    nal.bytes.assign(map.data, map.data + map.size);
    nal.pts_ns = GST_BUFFER_PTS_IS_VALID(buf) ? GST_BUFFER_PTS(buf) : 0;
    nal.dts_ns = GST_BUFFER_DTS_IS_VALID(buf) ? GST_BUFFER_DTS(buf) : 0;
    nal.duration_ns = GST_BUFFER_DURATION_IS_VALID(buf) ? GST_BUFFER_DURATION(buf) : 0;
    nal.is_keyframe = (GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DELTA_UNIT) == 0);
    gst_buffer_unmap(buf, &map);
    gst_sample_unref(sample);

    EncodedSink local;
    {
        std::lock_guard lock(mtx_);
        local = target;
    }
    if (local) {
        local(nal);
    }
    return GST_FLOW_OK;
}

auto GstEncoderPipeline::OnSegmentSampleStatic(GstAppSink* sink, gpointer user_data)
    -> GstFlowReturn {
    auto* self = static_cast<GstEncoderPipeline*>(user_data);
    return self->OnEncodedSample(sink, self->segment_sink_);
}

auto GstEncoderPipeline::OnRingSampleStatic(GstAppSink* sink, gpointer user_data)
    -> GstFlowReturn {
    auto* self = static_cast<GstEncoderPipeline*>(user_data);
    return self->OnEncodedSample(sink, self->ring_sink_);
}

}  // namespace sst::adapters::storage
