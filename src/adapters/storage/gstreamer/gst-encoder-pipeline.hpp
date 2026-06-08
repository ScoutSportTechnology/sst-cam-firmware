#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include "app/storage/ports/encoder-pipeline.hpp"

namespace sst::adapters::storage {

// One NVENC session shared by full-game recording AND event-clip extraction.
// The pipeline:
//
//   appsrc → videoconvert → nvvidconv → nvv4l2h264enc → h264parse → tee
//                                                                    ├─→ appsink (segment_sink)
//                                                                    └─→ appsink (ring_sink)
//
// Both appsinks receive identical encoded byte-stream H.264 buffers. The two
// downstream sinks are independent — pausing the segment recorder does not
// drain the ring sink, so event clips remain triggerable across pauses.
class GstEncoderPipeline final : public sst::storage::IEncoderPipeline {
   public:
    struct Config {
        // Caps for the appsrc (matches postprocess output by default).
        int width{1920};
        int height{1080};
        int framerate{30};
        std::string raw_format{"BGR"};
        // Encoder knobs.
        int bitrate_bps{4000000};
        int iframe_interval_frames{30};  // 1 keyframe per second at 30fps
    };

    explicit GstEncoderPipeline(Config config);
    ~GstEncoderPipeline() override;

    GstEncoderPipeline(const GstEncoderPipeline&) = delete;
    auto operator=(const GstEncoderPipeline&) -> GstEncoderPipeline& = delete;
    GstEncoderPipeline(GstEncoderPipeline&&) = delete;
    auto operator=(GstEncoderPipeline&&) -> GstEncoderPipeline& = delete;

    auto SetSegmentSink(EncodedSink sink) -> void override;
    auto SetRingSink(EncodedSink sink) -> void override;

    auto Start() -> bool override;
    auto Stop() -> void override;
    [[nodiscard]] auto IsRunning() const -> bool override;

    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    auto BuildPipeline() -> bool;
    auto Teardown() -> void;
    auto OnEncodedSample(GstAppSink* sink, EncodedSink& target) -> GstFlowReturn;

    static auto OnSegmentSampleStatic(GstAppSink* sink, gpointer user_data) -> GstFlowReturn;
    static auto OnRingSampleStatic(GstAppSink* sink, gpointer user_data) -> GstFlowReturn;

    Config config_;
    EncodedSink segment_sink_;
    EncodedSink ring_sink_;

    mutable std::mutex mtx_;
    std::atomic<bool> running_{false};

    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
    GstElement* segment_appsink_{nullptr};
    GstElement* ring_appsink_{nullptr};

    std::uint64_t pushed_frames_{0};
};

}  // namespace sst::adapters::storage
