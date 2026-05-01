#include "adapters/storage/gstreamer/gst-segment-recorder.hpp"

#include <algorithm>
#include <cstring>
#include <system_error>

#include <gst/app/gstappsrc.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sst::adapters::storage {

namespace {

constexpr const char* kAppsrcName = "src";
constexpr const char* kSplitmuxName = "mux";
constexpr const char* kSegmentTemplate = "seg_%05d.mp4";
constexpr const char* kMergedFile = "full_game.mp4";
constexpr GstClockTime kEosTimeout = 5 * GST_SECOND;

}  // namespace

GstSegmentRecorder::GstSegmentRecorder() { gst_init(nullptr, nullptr); }

GstSegmentRecorder::~GstSegmentRecorder() {
    if (running_) {
        (void)Stop();
    }
}

auto GstSegmentRecorder::Start(const std::filesystem::path& output_dir) -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        spdlog::error("GstSegmentRecorder::Start: mkdir({}) failed: {}", output_dir.string(),
                      ec.message());
        return false;
    }

    output_dir_ = output_dir;
    emitted_segments_.clear();
    paused_ = false;
    awaiting_idr_after_resume_ = false;

    const std::string location = (output_dir_ / kSegmentTemplate).string();
    const std::string launch = fmt::format(
        "appsrc name={src} is-live=true format=time do-timestamp=true "
        "  caps=\"video/x-h264,stream-format=byte-stream,alignment=au\" "
        " ! h264parse "
        " ! splitmuxsink name={mux} location=\"{loc}\" max-size-time=0 max-size-bytes=0 "
        "                muxer-factory=mp4mux send-keyframe-requests=true ",
        fmt::arg("src", kAppsrcName), fmt::arg("mux", kSplitmuxName),
        fmt::arg("loc", location));

    GError* err = nullptr;
    pipeline_ = gst_parse_launch(launch.c_str(), &err);
    if (err != nullptr) {
        spdlog::error("GstSegmentRecorder::Start: parse failed: {}", err->message);
        g_error_free(err);
        Teardown();
        return false;
    }
    if (pipeline_ == nullptr) {
        return false;
    }

    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), kAppsrcName);
    splitmuxsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), kSplitmuxName);
    if (appsrc_ == nullptr || splitmuxsink_ == nullptr) {
        spdlog::error("GstSegmentRecorder::Start: missing named elements");
        Teardown();
        return false;
    }

    g_signal_connect(splitmuxsink_, "format-location",
                     G_CALLBACK(&GstSegmentRecorder::OnFormatLocationStatic), this);

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstSegmentRecorder::Start: PLAYING transition failed");
        Teardown();
        return false;
    }

    running_ = true;
    spdlog::info("GstSegmentRecorder: started, segments at {}", location);
    return true;
}

auto GstSegmentRecorder::Pause() -> void {
    std::lock_guard lock(mtx_);
    if (!running_ || paused_) {
        return;
    }
    if (splitmuxsink_ != nullptr) {
        g_signal_emit_by_name(splitmuxsink_, "split-now");
    }
    paused_ = true;
    awaiting_idr_after_resume_ = false;
    spdlog::info("GstSegmentRecorder: Paused");
}

auto GstSegmentRecorder::Resume() -> void {
    std::lock_guard lock(mtx_);
    if (!running_ || !paused_) {
        return;
    }
    paused_ = false;
    awaiting_idr_after_resume_ = true;
    spdlog::info("GstSegmentRecorder: Resumed (awaiting next IDR)");
}

auto GstSegmentRecorder::Stop() -> std::filesystem::path {
    std::lock_guard lock(mtx_);
    if (!running_) {
        return {};
    }

    if (appsrc_ != nullptr) {
        gst_app_src_end_of_stream(GST_APP_SRC(appsrc_));
    }
    const bool eos_ok = WaitForEos();
    if (!eos_ok) {
        spdlog::warn("GstSegmentRecorder::Stop: EOS not received within timeout");
    }

    if (pipeline_ != nullptr) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }

    auto segments = CollectSegments();
    Teardown();
    running_ = false;
    paused_ = false;

    if (segments.empty()) {
        spdlog::error("GstSegmentRecorder::Stop: no segments produced");
        return {};
    }

    const auto merged = output_dir_ / kMergedFile;
    if (!merger_.Merge(segments, merged)) {
        spdlog::error("GstSegmentRecorder::Stop: merge failed");
        return {};
    }
    spdlog::info("GstSegmentRecorder: merged {} segments into {}", segments.size(),
                 merged.string());
    emitted_segments_ = std::move(segments);
    return merged;
}

auto GstSegmentRecorder::IsRunning() const -> bool { return running_; }
auto GstSegmentRecorder::IsPaused() const -> bool { return paused_; }

auto GstSegmentRecorder::Segments() const -> std::vector<std::filesystem::path> {
    std::lock_guard lock(mtx_);
    return emitted_segments_;
}

auto GstSegmentRecorder::OnEncoded(const sst::storage::EncodedNal& nal) -> void {
    if (!running_) {
        return;
    }
    if (paused_) {
        return;
    }
    if (awaiting_idr_after_resume_) {
        if (!nal.is_keyframe) {
            return;
        }
        awaiting_idr_after_resume_ = false;
    }

    GstElement* src = nullptr;
    {
        std::lock_guard lock(mtx_);
        src = appsrc_;
        if (src != nullptr) {
            gst_object_ref(src);
        }
    }
    if (src == nullptr) {
        return;
    }

    auto* gst_buf = gst_buffer_new_allocate(nullptr, nal.bytes.size(), nullptr);
    if (gst_buf != nullptr) {
        GstMapInfo map{};
        if (gst_buffer_map(gst_buf, &map, GST_MAP_WRITE) != 0) {
            std::memcpy(map.data, nal.bytes.data(), nal.bytes.size());
            gst_buffer_unmap(gst_buf, &map);
            GST_BUFFER_PTS(gst_buf) = nal.pts_ns;
            GST_BUFFER_DTS(gst_buf) = nal.dts_ns;
            GST_BUFFER_DURATION(gst_buf) =
                nal.duration_ns != 0 ? nal.duration_ns : GST_CLOCK_TIME_NONE;
            if (!nal.is_keyframe) {
                GST_BUFFER_FLAG_SET(gst_buf, GST_BUFFER_FLAG_DELTA_UNIT);
            }
            (void)gst_app_src_push_buffer(GST_APP_SRC(src), gst_buf);
        } else {
            gst_buffer_unref(gst_buf);
        }
    }

    gst_object_unref(src);
}

auto GstSegmentRecorder::Teardown() -> void {
    if (appsrc_ != nullptr) {
        gst_object_unref(appsrc_);
        appsrc_ = nullptr;
    }
    if (splitmuxsink_ != nullptr) {
        gst_object_unref(splitmuxsink_);
        splitmuxsink_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}

auto GstSegmentRecorder::WaitForEos() -> bool {
    if (pipeline_ == nullptr) {
        return false;
    }
    GstBus* bus = gst_element_get_bus(pipeline_);
    GstMessage* msg = gst_bus_timed_pop_filtered(
        bus, kEosTimeout,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    bool ok = false;
    if (msg != nullptr) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            ok = true;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    return ok;
}

auto GstSegmentRecorder::CollectSegments() const -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(output_dir_, ec)) {
        return out;
    }
    for (const auto& entry : std::filesystem::directory_iterator(output_dir_, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto fname = entry.path().filename().string();
        if (fname.rfind("seg_", 0) == 0 && entry.path().extension() == ".mp4") {
            out.push_back(entry.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

auto GstSegmentRecorder::OnFormatLocationStatic(GstElement* /*sink*/, guint fragment_id,
                                                gpointer user_data) -> gchararray {
    auto* self = static_cast<GstSegmentRecorder*>(user_data);
    const auto path =
        self->output_dir_ / fmt::format("seg_{:05d}.mp4", static_cast<unsigned>(fragment_id));
    return g_strdup(path.string().c_str());
}

}  // namespace sst::adapters::storage
