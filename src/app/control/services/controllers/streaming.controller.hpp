#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/streaming/ports/preview-server.hpp"

namespace sst::control {

// Routes "streaming.*" commands. Owns no GStreamer state — delegates to
// IPreviewServer. Verbs (provisional, until the .proto arrives):
//   - "start_preview" — begin the RTSP H.264 stream
//   - "stop_preview"  — tear it down
class StreamingController final : public IController {
   public:
    explicit StreamingController(sst::streaming::IPreviewServer& preview);

    auto Id() const -> std::string_view override { return "streaming"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::streaming::IPreviewServer& preview_;
};

}  // namespace sst::control
