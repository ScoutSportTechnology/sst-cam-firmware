#pragma once

#include "domain/capture/models/frame.hpp"
#include "domain/streaming/models/app-stream-config.hpp"

namespace sst::streaming {

// Always-on RTSP server consumed by the companion app over WiFi-Direct.
// Frames come from the orchestration thread via Push() — the adapter wraps
// each Frame in a GstBuffer, pushes it into its appsrc, and lets the gst-rtsp
// server downstream pipeline (NVENC + payloader) handle the rest.
class IAppStreamServer {
   public:
    virtual ~IAppStreamServer() = default;

    virtual auto Start(const AppStreamConfig& config) -> bool = 0;
    virtual auto Stop() -> void = 0;
    [[nodiscard]] virtual auto IsRunning() const -> bool = 0;

    virtual auto Push(const sst::capture::Frame& frame) -> void = 0;
};

}  // namespace sst::streaming
