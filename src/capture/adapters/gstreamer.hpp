#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "capture/ports/capture.hpp"
#include "config/domain/config_data.hpp"

extern "C" {
using GstElement = struct _GstElement;  // NOLINT(bugprone-reserved-identifier)
using GError = struct _GError;          // NOLINT(bugprone-reserved-identifier)
}

namespace sst::capture::adapters {

using sst::capture::domain::Frame;
using sst::capture::ports::ICaptureAdapter;
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