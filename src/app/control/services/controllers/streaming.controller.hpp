#pragma once

#include <cstdint>
#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/db/ports/stream-config-repository.hpp"
#include "app/streaming/ports/streaming-service.hpp"

namespace sst::control {

// Routes "streaming.*" BLE commands. Verbs (payload is plain text):
//
//   start_app_stream          → IStreamingService::StartAppStream(default config)
//   stop_app_stream           → IStreamingService::StopAppStream
//   start_platform_stream <id>→ look up stream_config by id, build a
//                               PlatformStreamConfig, hand to the service
//   stop_platform_stream <id> → IStreamingService::StopPlatformStream(id)
//   list_streams              → response payload =
//                               "<is_app_running>\n<id> <name>\n..."
//
// The DB stream_config row owns the per-platform settings (url, stream_key,
// codec, w/h/fps/bitrate); the controller reads the row at start time so
// edits to the DB take effect on the next start.
class StreamingController final : public IController {
   public:
    StreamingController(sst::streaming::IStreamingService& service,
                        sst::db::IStreamConfigRepository& stream_config_repo);

    auto Id() const -> std::string_view override { return "streaming"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::streaming::IStreamingService& service_;
    sst::db::IStreamConfigRepository& stream_config_repo_;
};

}  // namespace sst::control
