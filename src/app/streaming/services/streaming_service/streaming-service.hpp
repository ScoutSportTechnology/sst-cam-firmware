#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "app/buffer/ports/frame-sink.hpp"
#include "app/streaming/ports/app-stream-server.hpp"
#include "app/streaming/ports/platform-streamer.hpp"
#include "app/streaming/ports/streaming-service.hpp"

namespace sst::streaming {

// Owns the always-on RTSP companion-app server and a map of active platform
// streamers keyed by stream_id. Implements both IStreamingService (for the BLE
// controller) and IFrameSink (for the orchestration thread). Push() fans every
// frame out to the app server (when running) and each active platform stream.
//
// Construction takes a factory for IPlatformStreamer because each active
// destination needs its own NVENC session + appsrc — the service can't share
// one streamer across destinations.
class StreamingService final : public IStreamingService, public sst::buffer::IFrameSink {
   public:
    using PlatformStreamerFactory = std::function<std::unique_ptr<IPlatformStreamer>()>;

    StreamingService(std::unique_ptr<IAppStreamServer> app_stream_server,
                     PlatformStreamerFactory platform_streamer_factory);
    ~StreamingService() override;

    StreamingService(const StreamingService&) = delete;
    auto operator=(const StreamingService&) -> StreamingService& = delete;
    StreamingService(StreamingService&&) = delete;
    auto operator=(StreamingService&&) -> StreamingService& = delete;

    // IStreamingService
    auto StartAppStream(const AppStreamConfig& config) -> bool override;
    auto StopAppStream() -> bool override;
    [[nodiscard]] auto IsAppStreamRunning() const -> bool override;

    auto StartPlatformStream(const PlatformStreamConfig& config) -> bool override;
    auto StopPlatformStream(std::int64_t stream_id) -> bool override;

    [[nodiscard]] auto ListActivePlatformStreams() const
        -> std::vector<ActivePlatformStream> override;

    // IFrameSink
    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    std::unique_ptr<IAppStreamServer> app_stream_server_;
    PlatformStreamerFactory platform_streamer_factory_;

    mutable std::mutex mtx_;
    std::unordered_map<std::int64_t, std::unique_ptr<IPlatformStreamer>> platform_streams_;
    // Mirror of the platform_streams_ keys + names so ListActive can run
    // without locking the underlying streamer instances.
    std::unordered_map<std::int64_t, std::string> active_names_;
};

}  // namespace sst::streaming
