#include "adapters/streaming/gst_rtmp/gst-rtmp-streamer.hpp"

#include <cstring>
#include <string>

#include <gst/app/gstappsrc.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "domain/streaming/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::adapters::streaming {

namespace {

constexpr const char* kAppsrcName = "src";

auto FrameByteSize(const sst::capture::Frame& frame) -> std::size_t {
    std::size_t total = 0;
    for (const auto& plane : frame.planes) {
        total += plane.size;
    }
    return total;
}

auto BuildLocation(const sst::streaming::PlatformStreamConfig& cfg) -> std::string {
    // rtmpsink expects "<url>/<key>" — most ingest endpoints accept the key
    // as the last path segment. RTMPS is the same element, just different
    // URL scheme; PlatformStreamType only affects how we build it.
    if (cfg.url.empty()) {
        return cfg.stream_key;
    }
    if (cfg.url.back() == '/') {
        return cfg.url + cfg.stream_key;
    }
    return cfg.url + "/" + cfg.stream_key;
}

auto BuildLaunch(const sst::streaming::PlatformStreamConfig& cfg) -> std::string {
    // Software H.264 (the Orin Nano has no NVENC): x264enc reads system memory,
    // so the nvvidconv→NVMM hop is dropped. rtmp2sink replaces the deprecated
    // rtmpsink — it takes a clean location URL (no embedded " live=1"). The
    // uplink queue is leaky-downstream + non-blocking so a stalled RTMP socket
    // can never back-pressure the capture/encode path. x264enc bitrate is in
    // kbit/s. do-timestamp=true on the appsrc supplies valid PTS (x264enc
    // requires it). flvmux is named so U9 can attach a silent AAC audio pad
    // (platforms like YouTube require an audio track). A pad-blocking bus-error
    // reconnect is handled in U9 (rtmp2sink has no auto-reconnect).
    return fmt::format(
        "appsrc name={src} is-live=true format=time do-timestamp=true "
        "  caps=\"video/x-raw,format=BGR,width={w},height={h},framerate={fps}/1\" "
        " ! videoconvert "
        " ! x264enc speed-preset=ultrafast tune=zerolatency bitrate={brk} key-int-max={gik} "
        " ! h264parse config-interval=-1 "
        " ! queue leaky=downstream max-size-buffers=3 "
        " ! flvmux name=mux streamable=true "
        " ! rtmp2sink location=\"{loc}\" sync=false ",
        fmt::arg("src", kAppsrcName), fmt::arg("w", cfg.width), fmt::arg("h", cfg.height),
        fmt::arg("fps", cfg.framerate), fmt::arg("brk", cfg.bitrate_kbps),
        fmt::arg("gik", cfg.framerate * 2), fmt::arg("loc", BuildLocation(cfg)));
}

}  // namespace

GstRtmpStreamer::GstRtmpStreamer() { gst_init(nullptr, nullptr); }

GstRtmpStreamer::~GstRtmpStreamer() {
    if (running_) {
        Stop();
    }
}

auto GstRtmpStreamer::Start(const sst::streaming::PlatformStreamConfig& config) -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        return true;
    }
    if (config.url.empty() || config.stream_key.empty()) {
        spdlog::error("GstRtmpStreamer::Start rejected: url/stream_key required ({})", config);
        return false;
    }
    config_ = config;
    spdlog::info("GstRtmpStreamer::Start {}", config_);

    const std::string launch = BuildLaunch(config_);
    GError* err = nullptr;
    pipeline_ = gst_parse_launch(launch.c_str(), &err);
    if (err != nullptr) {
        spdlog::error("GstRtmpStreamer::Start: parse failed: {}", err->message);
        g_error_free(err);
        Teardown();
        return false;
    }
    if (pipeline_ == nullptr) {
        return false;
    }
    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), kAppsrcName);
    if (appsrc_ == nullptr) {
        spdlog::error("GstRtmpStreamer::Start: appsrc not found");
        Teardown();
        return false;
    }
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstRtmpStreamer::Start: PLAYING transition failed");
        Teardown();
        return false;
    }
    running_ = true;
    return true;
}

auto GstRtmpStreamer::Stop() -> void {
    std::lock_guard lock(mtx_);
    if (!running_) {
        return;
    }
    if (appsrc_ != nullptr) {
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));
    }
    if (pipeline_ != nullptr) {
        // Wait for EOS (or error) so flvmux/rtmpsink flush the tail of the
        // stream before we cut the pipeline — otherwise the last GOP is lost.
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
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    Teardown();
    running_ = false;
    spdlog::info("GstRtmpStreamer::Stop id={}", config_.stream_id);
}

auto GstRtmpStreamer::IsRunning() const -> bool { return running_; }
auto GstRtmpStreamer::Id() const -> std::int64_t { return config_.stream_id; }

auto GstRtmpStreamer::Teardown() -> void {
    if (appsrc_ != nullptr) {
        gst_object_unref(appsrc_);
        appsrc_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}

auto GstRtmpStreamer::Push(const sst::capture::Frame& frame) -> void {
    GstElement* src = nullptr;
    {
        std::lock_guard lock(mtx_);
        if (!running_ || appsrc_ == nullptr) {
            return;
        }
        src = appsrc_;
        gst_object_ref(src);
    }

    const auto total = FrameByteSize(frame);
    if (total == 0) {
        gst_object_unref(src);
        return;
    }

    auto* gst_buf = gst_buffer_new_allocate(nullptr, total, nullptr);
    if (gst_buf == nullptr) {
        gst_object_unref(src);
        return;
    }
    GstMapInfo map{};
    if (gst_buffer_map(gst_buf, &map, GST_MAP_WRITE) == 0) {
        gst_buffer_unref(gst_buf);
        gst_object_unref(src);
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

    // Leave timestamps unset so the do-timestamp=true appsrc stamps a valid
    // running-time PTS. Forcing GST_CLOCK_TIME_NONE corrupts the x264enc
    // software-encoded stream (nvv4l2h264enc tolerated it; x264enc does not).

    (void)gst_app_src_push_buffer(GST_APP_SRC(src), gst_buf);
    gst_object_unref(src);
}

}  // namespace sst::adapters::streaming
