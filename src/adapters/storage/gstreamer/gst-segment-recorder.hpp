#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <vector>

#include <gst/gst.h>

#include "adapters/storage/gstreamer/gst-segment-merger.hpp"
#include "app/storage/ports/segment-recorder.hpp"

namespace sst::adapters::storage {

// Wraps `splitmuxsink` to rotate one mp4 segment per pause→resume cycle.
//
// Pipeline:
//   appsrc → h264parse → splitmuxsink (location=seg_%05d.mp4, max-size-time=0)
//
// Pause closes the current segment via the `split-now` signal and starts
// dropping incoming buffers. Resume drops buffers until the next IDR (the
// upstream encoder is configured with iframeinterval=fps so a keyframe is at
// most ~1 second away), then forwards them, which causes splitmuxsink to open
// a fresh segment file.
//
// Stop closes the current segment, sends EOS, waits for it to land, then
// runs `GstSegmentMerger` over the produced segments and returns the merged
// path.
class GstSegmentRecorder final : public sst::storage::ISegmentRecorder {
   public:
    GstSegmentRecorder();
    ~GstSegmentRecorder() override;

    GstSegmentRecorder(const GstSegmentRecorder&) = delete;
    auto operator=(const GstSegmentRecorder&) -> GstSegmentRecorder& = delete;
    GstSegmentRecorder(GstSegmentRecorder&&) = delete;
    auto operator=(GstSegmentRecorder&&) -> GstSegmentRecorder& = delete;

    auto Start(const std::filesystem::path& output_dir) -> bool override;
    auto Pause() -> void override;
    auto Resume() -> void override;
    auto Stop() -> std::filesystem::path override;
    [[nodiscard]] auto IsRunning() const -> bool override;
    [[nodiscard]] auto IsPaused() const -> bool override;
    [[nodiscard]] auto Segments() const -> std::vector<std::filesystem::path> override;

    auto OnEncoded(const sst::storage::EncodedNal& nal) -> void override;

   private:
    auto Teardown() -> void;
    auto WaitForEos() -> bool;
    auto CollectSegments() const -> std::vector<std::filesystem::path>;

    static auto OnFormatLocationStatic(GstElement* sink, guint fragment_id, gpointer user_data)
        -> gchararray;

    GstSegmentMerger merger_;

    mutable std::mutex mtx_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    // After Pause / split-now we drop until the next IDR so splitmuxsink can
    // open a clean fragment.
    std::atomic<bool> awaiting_idr_after_resume_{false};

    std::filesystem::path output_dir_;
    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
    GstElement* splitmuxsink_{nullptr};

    std::vector<std::filesystem::path> emitted_segments_;
};

}  // namespace sst::adapters::storage
