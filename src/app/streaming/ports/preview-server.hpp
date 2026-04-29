#pragma once

#include "domain/streaming/models/preview-config.hpp"

namespace sst::streaming {

// Low-bitrate live preview server. Concrete adapter (gst-rtsp-server) owns the
// GStreamer pipeline; the StreamingController toggles it on/off in response to
// BLE commands from the phone.
class IPreviewServer {
   public:
    virtual ~IPreviewServer() = default;

    virtual auto Start(const PreviewConfig& config) -> bool = 0;
    virtual auto Stop() -> void = 0;
    virtual auto IsRunning() const -> bool = 0;
};

}  // namespace sst::streaming
