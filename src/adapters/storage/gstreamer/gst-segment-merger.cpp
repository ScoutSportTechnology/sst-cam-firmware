#include "adapters/storage/gstreamer/gst-segment-merger.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <system_error>

#include <gst/gst.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sst::adapters::storage {

namespace {

constexpr GstClockTime kStateChangeTimeout = 10 * GST_SECOND;

auto BuildLaunch(const std::vector<std::filesystem::path>& segments,
                 const std::filesystem::path& output) -> std::string {
    std::ostringstream os;
    os << "concat name=c ";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        os << " filesrc location=\"" << segments[i].string() << "\" "
           << " ! qtdemux ! h264parse ! c.sink_" << i;
    }
    os << " c. ! h264parse ! mp4mux ! filesink location=\"" << output.string() << "\" ";
    return os.str();
}

}  // namespace

GstSegmentMerger::GstSegmentMerger() { gst_init(nullptr, nullptr); }

auto GstSegmentMerger::Merge(const std::vector<std::filesystem::path>& segments,
                             const std::filesystem::path& output) -> bool {
    if (segments.empty()) {
        spdlog::error("GstSegmentMerger: no segments to merge");
        return false;
    }
    for (const auto& seg : segments) {
        std::error_code ec;
        if (!std::filesystem::exists(seg, ec) || ec) {
            spdlog::error("GstSegmentMerger: missing segment {}", seg.string());
            return false;
        }
    }

    const std::string launch = BuildLaunch(segments, output);
    spdlog::info("GstSegmentMerger: launch = {}", launch);

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(launch.c_str(), &err);
    if (err != nullptr) {
        spdlog::error("GstSegmentMerger: parse failed: {}", err->message);
        g_error_free(err);
        if (pipeline != nullptr) {
            gst_object_unref(pipeline);
        }
        return false;
    }
    if (pipeline == nullptr) {
        return false;
    }

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("GstSegmentMerger: failed to set PLAYING");
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return false;
    }

    GstBus* bus = gst_element_get_bus(pipeline);
    bool ok = false;
    GstMessage* msg = gst_bus_timed_pop_filtered(
        bus, kStateChangeTimeout,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    if (msg == nullptr) {
        spdlog::error("GstSegmentMerger: timeout waiting for EOS");
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        ok = true;
    } else {
        GError* gerr = nullptr;
        gchar* dbg = nullptr;
        gst_message_parse_error(msg, &gerr, &dbg);
        spdlog::error("GstSegmentMerger: error: {} ({})", gerr != nullptr ? gerr->message : "?",
                      dbg != nullptr ? dbg : "");
        if (gerr != nullptr) {
            g_error_free(gerr);
        }
        if (dbg != nullptr) {
            g_free(dbg);
        }
    }
    if (msg != nullptr) {
        gst_message_unref(msg);
    }
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return ok;
}

}  // namespace sst::adapters::storage
