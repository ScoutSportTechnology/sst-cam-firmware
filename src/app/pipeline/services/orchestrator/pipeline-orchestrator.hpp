#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "app/buffer/ports/frame-sink.hpp"
#include "app/capture/ports/frame-src.hpp"
#include "app/processing/ports/postprocessor.hpp"
#include "app/processing/ports/preprocessor.hpp"
#include "domain/buffer/services/latest-only-slot.hpp"
#include "domain/processing/models/frame-bundle.hpp"

namespace sst::pipeline {

struct PipelineConfig {
    // Polling interval used by the producer when Capture() returns nullopt
    // (no new frame ready). Tight enough to not miss frames at 30+ fps,
    // loose enough not to spin a core when the camera is idle.
    std::chrono::milliseconds capture_idle_sleep{5};
    // Bound on how long the consumer waits for a new bundle before checking
    // its run flag. Keeps shutdown latency low.
    std::chrono::milliseconds consumer_pop_timeout{100};
};

// Single-camera pipeline orchestrator. Owns two threads:
//
//   producer: ICaptureFrame::Capture() in a loop → IPreprocessor::Process →
//             buffer::MaterializeFrame(source) → push to LatestOnlySlot.
//
//   consumer: pop from slot → IPostprocessor::Process(source, full-frame crop)
//             → IFrameSink::Push.
//
// The full-frame crop is the "no-AI" default. Once the AI/physics/decision
// modules land they replace the crop selection, slotting in between the
// LatestOnlySlot pop and the postprocessor call.
//
// Threading: producer and consumer are independent threads. The
// LatestOnlySlot drops older bundles on overrun, decoupling capture cadence
// from postprocess+sink cadence so a slow downstream sink can't back-pressure
// the camera.
class PipelineOrchestrator {
   public:
    PipelineOrchestrator(std::unique_ptr<sst::capture::ICaptureFrame> capture,
                         std::unique_ptr<sst::processing::IPreprocessor> preprocessor,
                         std::unique_ptr<sst::processing::IPostprocessor> postprocessor,
                         sst::buffer::IFrameSink& sink, PipelineConfig config = PipelineConfig{});

    ~PipelineOrchestrator();

    PipelineOrchestrator(const PipelineOrchestrator&) = delete;
    auto operator=(const PipelineOrchestrator&) -> PipelineOrchestrator& = delete;
    PipelineOrchestrator(PipelineOrchestrator&&) = delete;
    auto operator=(PipelineOrchestrator&&) -> PipelineOrchestrator& = delete;

    // Idempotent. Starts the underlying capture, then spawns both threads.
    auto Start() -> bool;
    // Idempotent. Signals both threads to exit, joins them, stops the capture.
    auto Stop() -> void;
    [[nodiscard]] auto IsRunning() const -> bool;

   private:
    auto ProducerLoop() -> void;
    auto ConsumerLoop() -> void;

    std::unique_ptr<sst::capture::ICaptureFrame> capture_;
    std::unique_ptr<sst::processing::IPreprocessor> preprocessor_;
    std::unique_ptr<sst::processing::IPostprocessor> postprocessor_;
    sst::buffer::IFrameSink& sink_;
    PipelineConfig config_;

    sst::buffer::LatestOnlySlot<sst::processing::FrameBundle> slot_;

    mutable std::mutex mtx_;
    std::atomic<bool> running_{false};
    std::thread producer_thread_;
    std::thread consumer_thread_;
};

}  // namespace sst::pipeline
