#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "app/capture/ports/frame-src.hpp"
#include "domain/config/models/device-model.hpp"
#include "domain/db/models/camera.hpp"

extern "C" {
using GstElement = struct _GstElement;  // NOLINT(bugprone-reserved-identifier)
using GError = struct _GError;          // NOLINT(bugprone-reserved-identifier)
using GstBus = struct _GstBus;          // NOLINT(bugprone-reserved-identifier)
using GstSample = struct _GstSample;    // NOLINT(bugprone-reserved-identifier)
}

namespace sst::capture {

using sst::capture::Frame;
using sst::capture::ICaptureFrame;

class GStreamerAdapter final : public ICaptureFrame {
   public:
    GStreamerAdapter(const sst::db::CameraConfig& camera_config,
                     sst::config::DeviceModel device_model, std::uint16_t camera_index);

    ~GStreamerAdapter() override = default;

    auto Start() -> void override;
    auto Stop() -> void override;
    auto IsRunning() const -> bool override;
    auto Capture() -> std::optional<Frame> override;

   private:
    sst::db::CameraConfig camera_config_;
    sst::config::DeviceModel device_model_;
    std::uint16_t camera_index_{0};
    std::uint64_t frame_counter_{0};
    bool is_running_{false};
    std::string pipeline_str_;
    GstElement* gst_pipeline_{nullptr};
    GstBus* gst_bus_{nullptr};
    GError* gst_err_{nullptr};
    GstElement* gst_sink_{nullptr};
    std::string gst_sink_name_{"sink"};

    auto CleanupPipeline() -> void;
    auto HandleBusMessages() -> bool;
    auto CreateFrameFromSample(GstSample* gst_sample) -> std::optional<Frame>;
    auto CreatePipeline() -> std::string;

    mutable std::mutex last_frame_mtx_;
    std::optional<sst::capture::Frame> last_frame_;
};

}  // namespace sst::capture
