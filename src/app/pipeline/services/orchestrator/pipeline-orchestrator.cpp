#include "app/pipeline/services/orchestrator/pipeline-orchestrator.hpp"

#include <utility>

#include <spdlog/spdlog.h>

#include "domain/buffer/services/materialize-frame.hpp"
#include "domain/processing/models/crop-rect.hpp"

namespace sst::pipeline {

PipelineOrchestrator::PipelineOrchestrator(
    std::unique_ptr<sst::capture::ICaptureFrame> capture,
    std::unique_ptr<sst::processing::IPreprocessor> preprocessor,
    std::unique_ptr<sst::processing::IPostprocessor> postprocessor,
    sst::buffer::IFrameSink& sink, PipelineConfig config)
    : capture_(std::move(capture)),
      preprocessor_(std::move(preprocessor)),
      postprocessor_(std::move(postprocessor)),
      sink_(sink),
      config_(config) {}

PipelineOrchestrator::~PipelineOrchestrator() {
    if (running_) {
        Stop();
    }
}

auto PipelineOrchestrator::Start() -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        return true;
    }

    capture_->Start();
    if (!capture_->IsRunning()) {
        spdlog::error("PipelineOrchestrator::Start: capture failed to start");
        return false;
    }

    running_ = true;
    producer_thread_ = std::thread(&PipelineOrchestrator::ProducerLoop, this);
    consumer_thread_ = std::thread(&PipelineOrchestrator::ConsumerLoop, this);
    spdlog::info("PipelineOrchestrator: started");
    return true;
}

auto PipelineOrchestrator::Stop() -> void {
    std::thread to_join_producer;
    std::thread to_join_consumer;
    {
        std::lock_guard lock(mtx_);
        if (!running_) {
            return;
        }
        running_ = false;
        slot_.Close();
        to_join_producer = std::move(producer_thread_);
        to_join_consumer = std::move(consumer_thread_);
    }
    if (to_join_producer.joinable()) {
        to_join_producer.join();
    }
    if (to_join_consumer.joinable()) {
        to_join_consumer.join();
    }
    capture_->Stop();
    spdlog::info("PipelineOrchestrator: stopped");
}

auto PipelineOrchestrator::IsRunning() const -> bool { return running_; }

auto PipelineOrchestrator::ProducerLoop() -> void {
    while (running_) {
        auto raw = capture_->Capture();
        if (!raw) {
            std::this_thread::sleep_for(config_.capture_idle_sleep);
            continue;
        }
        auto bundle = preprocessor_->Process(*raw);
        if (!bundle) {
            // Preprocess failure is logged inside Process(); drop and continue.
            continue;
        }
        // Materialize the source plane bytes off the GstBuffer so the appsink
        // slot is freed before we hand the bundle to the consumer thread.
        bundle->source_frame = sst::buffer::MaterializeFrame(bundle->source_frame);
        slot_.Push(std::move(*bundle));
    }
}

auto PipelineOrchestrator::ConsumerLoop() -> void {
    while (running_) {
        auto bundle = slot_.Pop(config_.consumer_pop_timeout);
        if (!bundle) {
            continue;
        }
        const sst::processing::CropRect full_frame{
            .x = 0,
            .y = 0,
            .width = bundle->source_frame.geometry.width,
            .height = bundle->source_frame.geometry.height,
        };
        auto final_frame = postprocessor_->Process(bundle->source_frame, full_frame);
        if (!final_frame) {
            continue;
        }
        {
            // Retain the latest final frame for on-demand snapshots. It already
            // owns its pixels (postprocessor MakeOwnedFrame), so storing it by
            // value keeps those bytes alive for a later GrabLatest().
            std::lock_guard lock(latest_frame_mtx_);
            latest_frame_ = *final_frame;
        }
        sink_.Push(*final_frame);
    }
}

auto PipelineOrchestrator::GrabLatest() -> std::optional<sst::capture::Frame> {
    std::lock_guard lock(latest_frame_mtx_);
    return latest_frame_;
}

}  // namespace sst::pipeline
