#pragma once

#include <cstdint>

#include "domain/capture/models/frame.hpp"
#include "domain/streaming/models/platform-stream-config.hpp"

namespace sst::streaming {

// One RTMP/RTMPS push to a platform (YouTube / Twitch / Facebook / Instagram /
// custom). Each active destination has its own streamer instance with its own
// NVENC session and its own appsrc — the streaming service owns a map of them
// keyed by `stream_id`.
class IPlatformStreamer {
   public:
    virtual ~IPlatformStreamer() = default;

    virtual auto Start(const PlatformStreamConfig& config) -> bool = 0;
    virtual auto Stop() -> void = 0;
    [[nodiscard]] virtual auto IsRunning() const -> bool = 0;
    [[nodiscard]] virtual auto Id() const -> std::int64_t = 0;

    virtual auto Push(const sst::capture::Frame& frame) -> void = 0;
};

}  // namespace sst::streaming
