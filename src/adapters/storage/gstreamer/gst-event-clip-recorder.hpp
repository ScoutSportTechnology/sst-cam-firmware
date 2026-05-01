#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

#include <gst/gst.h>

#include "app/storage/ports/event-clip-recorder.hpp"

namespace sst::adapters::storage {

// Maintains a rolling pre-event ring of encoded H.264 NALs (sized by PTS, not
// frame count) and, on Trigger, dumps a snapshot starting at the most recent
// IDR within `window.pre_seconds` into a fresh `mp4mux → filesink` pipeline.
// Continues forwarding live NALs into the same pipeline for `window.post_seconds`,
// then sends EOS asynchronously on a finalize worker.
class GstEventClipRecorder final : public sst::storage::IEventClipRecorder {
   public:
    struct Config {
        // Upper bound on the rolling ring. Older NALs are pruned on push so
        // the ring never exceeds `max_pre_seconds` worth of stream.
        std::int64_t max_pre_seconds{120};
    };

    explicit GstEventClipRecorder(Config config);
    ~GstEventClipRecorder() override;

    GstEventClipRecorder(const GstEventClipRecorder&) = delete;
    auto operator=(const GstEventClipRecorder&) -> GstEventClipRecorder& = delete;
    GstEventClipRecorder(GstEventClipRecorder&&) = delete;
    auto operator=(GstEventClipRecorder&&) -> GstEventClipRecorder& = delete;

    auto OnEncoded(const sst::storage::EncodedNal& nal) -> void override;
    auto Trigger(const std::filesystem::path& output_path,
                 const sst::storage::EventClipWindow& window) -> bool override;
    [[nodiscard]] auto IsBusy() const -> bool override;

   private:
    auto PruneRing() -> void;
    auto PushNalToPipeline(const sst::storage::EncodedNal& nal) -> void;
    auto BuildPipeline(const std::filesystem::path& output_path) -> bool;
    auto JoinFinalizeWorker() -> void;
    static auto FinalizePipeline(GstElement* pipeline, GstElement* appsrc) -> void;

    Config config_;

    mutable std::mutex mtx_;
    std::deque<sst::storage::EncodedNal> ring_;

    std::atomic<bool> busy_{false};
    std::uint64_t post_until_pts_ns_{0};
    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};

    std::optional<std::thread> finalize_worker_;
};

}  // namespace sst::adapters::storage
