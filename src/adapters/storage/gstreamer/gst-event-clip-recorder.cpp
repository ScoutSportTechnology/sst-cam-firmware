#include "adapters/storage/gstreamer/gst-event-clip-recorder.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include <gst/app/gstappsrc.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sst::adapters::storage {

namespace {

constexpr std::uint64_t kNanosPerSecond = 1'000'000'000ULL;
constexpr GstClockTime kEosTimeout = 10 * GST_SECOND;
constexpr const char* kAppsrcName = "src";

}  // namespace

GstEventClipRecorder::GstEventClipRecorder(Config config) : config_(config) {
    gst_init(nullptr, nullptr);
}

GstEventClipRecorder::~GstEventClipRecorder() { JoinFinalizeWorker(); }

auto GstEventClipRecorder::OnEncoded(const sst::storage::EncodedNal& nal) -> void {
    std::lock_guard lock(mtx_);
    ring_.push_back(nal);
    PruneRing();

    if (!busy_ || pipeline_ == nullptr || appsrc_ == nullptr) {
        return;
    }

    PushNalToPipeline(nal);

    if (nal.pts_ns >= post_until_pts_ns_) {
        // The post-event window has elapsed. Hand the pipeline off to a
        // finalize worker so the bus wait + state-NULL transition don't block
        // the encoder's signal thread.
        GstElement* p = pipeline_;
        GstElement* src = appsrc_;
        pipeline_ = nullptr;
        appsrc_ = nullptr;
        post_until_pts_ns_ = 0;
        busy_ = false;

        if (finalize_worker_ && finalize_worker_->joinable()) {
            // Detach the previous worker; we already handed off its pipeline
            // earlier and it'll terminate when the bus pop times out / EOS
            // arrives. Joining here from inside OnEncoded would deadlock if
            // the previous worker is itself blocked on the bus.
            finalize_worker_->detach();
        }
        finalize_worker_ = std::thread(&GstEventClipRecorder::FinalizePipeline, p, src);
    }
}

auto GstEventClipRecorder::Trigger(const std::filesystem::path& output_path,
                                   const sst::storage::EventClipWindow& window) -> bool {
    std::lock_guard lock(mtx_);
    if (busy_) {
        spdlog::warn("GstEventClipRecorder::Trigger rejected: previous clip still recording");
        return false;
    }
    if (window.pre_seconds <= 0 || window.post_seconds <= 0) {
        return false;
    }
    if (ring_.empty()) {
        spdlog::warn("GstEventClipRecorder::Trigger rejected: ring is empty (encoder hasn't "
                     "emitted any NALs yet)");
        return false;
    }

    const std::uint64_t now_pts = ring_.back().pts_ns;
    const std::uint64_t pre_ns = static_cast<std::uint64_t>(window.pre_seconds) * kNanosPerSecond;
    const std::uint64_t cutoff = now_pts > pre_ns ? now_pts - pre_ns : 0;

    // Find the most recent IDR at or after the cutoff. If none exists in the
    // window, fall back to the oldest IDR we have (the clip will start
    // earlier than the user requested but at least it'll decode cleanly).
    auto idr_it =
        std::find_if(ring_.rbegin(), ring_.rend(), [cutoff](const sst::storage::EncodedNal& n) {
            return n.is_keyframe && n.pts_ns >= cutoff;
        });
    if (idr_it == ring_.rend()) {
        idr_it = std::find_if(ring_.rbegin(), ring_.rend(),
                              [](const sst::storage::EncodedNal& n) { return n.is_keyframe; });
    }
    if (idr_it == ring_.rend()) {
        spdlog::warn("GstEventClipRecorder::Trigger rejected: no IDR in ring");
        return false;
    }
    const std::uint64_t idr_pts = idr_it->pts_ns;

    if (!BuildPipeline(output_path)) {
        return false;
    }

    // Replay snapshot from the chosen IDR forward.
    for (const auto& nal : ring_) {
        if (nal.pts_ns >= idr_pts) {
            PushNalToPipeline(nal);
        }
    }

    post_until_pts_ns_ =
        now_pts + static_cast<std::uint64_t>(window.post_seconds) * kNanosPerSecond;
    busy_ = true;
    spdlog::info(
        "GstEventClipRecorder: triggered clip → {} (pre={}s post={}s, snapshot starts at "
        "PTS={}ns, target end PTS={}ns)",
        output_path.string(), window.pre_seconds, window.post_seconds, idr_pts,
        post_until_pts_ns_);
    return true;
}

auto GstEventClipRecorder::IsBusy() const -> bool { return busy_; }

auto GstEventClipRecorder::PruneRing() -> void {
    if (ring_.empty()) {
        return;
    }
    const std::uint64_t newest = ring_.back().pts_ns;
    const std::uint64_t window_ns =
        static_cast<std::uint64_t>(config_.max_pre_seconds) * kNanosPerSecond;
    const std::uint64_t cutoff = newest > window_ns ? newest - window_ns : 0;
    while (!ring_.empty() && ring_.front().pts_ns < cutoff) {
        ring_.pop_front();
    }
}

auto GstEventClipRecorder::PushNalToPipeline(const sst::storage::EncodedNal& nal) -> void {
    if (appsrc_ == nullptr) {
        return;
    }
    auto* gst_buf = gst_buffer_new_allocate(nullptr, nal.bytes.size(), nullptr);
    if (gst_buf == nullptr) {
        return;
    }
    GstMapInfo map{};
    if (gst_buffer_map(gst_buf, &map, GST_MAP_WRITE) == 0) {
        gst_buffer_unref(gst_buf);
        return;
    }
    std::memcpy(map.data, nal.bytes.data(), nal.bytes.size());
    gst_buffer_unmap(gst_buf, &map);
    GST_BUFFER_PTS(gst_buf) = nal.pts_ns;
    GST_BUFFER_DTS(gst_buf) = nal.dts_ns;
    GST_BUFFER_DURATION(gst_buf) =
        nal.duration_ns != 0 ? nal.duration_ns : GST_CLOCK_TIME_NONE;
    if (!nal.is_keyframe) {
        GST_BUFFER_FLAG_SET(gst_buf, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    (void)gst_app_src_push_buffer(GST_APP_SRC(appsrc_), gst_buf);
}

auto GstEventClipRecorder::BuildPipeline(const std::filesystem::path& output_path) -> bool {
    const std::string launch = fmt::format(
        "appsrc name={src} is-live=false format=time "
        "  caps=\"video/x-h264,stream-format=byte-stream,alignment=au\" "
        " ! h264parse "
        " ! mp4mux faststart=true "
        " ! filesink location=\"{loc}\" sync=false ",
        fmt::arg("src", kAppsrcName), fmt::arg("loc", output_path.string()));

    GError* err = nullptr;
    pipeline_ = gst_parse_launch(launch.c_str(), &err);
    if (err != nullptr) {
        spdlog::error("GstEventClipRecorder: parse failed: {}", err->message);
        g_error_free(err);
        if (pipeline_ != nullptr) {
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
        }
        return false;
    }
    if (pipeline_ == nullptr) {
        return false;
    }
    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), kAppsrcName);
    if (appsrc_ == nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstEventClipRecorder: PLAYING transition failed");
        gst_object_unref(appsrc_);
        gst_object_unref(pipeline_);
        appsrc_ = nullptr;
        pipeline_ = nullptr;
        return false;
    }
    return true;
}

auto GstEventClipRecorder::FinalizePipeline(GstElement* pipeline, GstElement* appsrc) -> void {
    gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* msg = gst_bus_timed_pop_filtered(
        bus, kEosTimeout,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    if (msg != nullptr) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* gerr = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &gerr, &dbg);
            spdlog::error("GstEventClipRecorder finalize error: {} ({})",
                          gerr != nullptr ? gerr->message : "?", dbg != nullptr ? dbg : "");
            if (gerr != nullptr) {
                g_error_free(gerr);
            }
            if (dbg != nullptr) {
                g_free(dbg);
            }
        }
        gst_message_unref(msg);
    } else {
        spdlog::warn("GstEventClipRecorder finalize: EOS timeout");
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsrc);
    gst_object_unref(pipeline);
}

auto GstEventClipRecorder::JoinFinalizeWorker() -> void {
    std::optional<std::thread> w;
    {
        std::lock_guard lock(mtx_);
        w = std::move(finalize_worker_);
        finalize_worker_.reset();
    }
    if (w && w->joinable()) {
        w->join();
    }
}

}  // namespace sst::adapters::storage
