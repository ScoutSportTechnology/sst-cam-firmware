#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstdint>
#include <optional>

#include "capture/ports/capture.hpp"
#include "common/domain/memory_type.hpp"
#include "common/domain/pixel_format.hpp"
#include "config/domain/config_data.hpp"

namespace sst::capture::adapters {

using sst::capture::domain::Frame;
using sst::capture::domain::FrameGeometry;
using sst::capture::domain::FramePlane;
using sst::capture::ports::ICaptureAdapter;
using sst::common::domain::MemoryType;
using sst::common::domain::PixelFormat;
using sst::config::domain::ConfigData;

class GStreamerAdapter final : public ICaptureAdapter {
   public:
    GStreamerAdapter(const ConfigData& config_data, std::uint16_t camera_index)
        : config_data_(config_data), camera_index_(camera_index) {}

    ~GStreamerAdapter() override = default;

    auto Start() -> void override;
    auto Stop() -> void override;
    auto IsRunning() const -> bool override;
    auto Capture() -> std::optional<Frame> override;

   private:
    const ConfigData& config_data_;
    std::uint16_t camera_index_{0};
    std::uint64_t frame_counter_{0};
    bool is_running_{false};
    std::string pipeline_str_;
    GstElement* gst_pipeline_{nullptr};
    GError* gst_err_{nullptr};
    GstElement* gst_sink_{nullptr};
};

}  // namespace sst::capture::adapters
