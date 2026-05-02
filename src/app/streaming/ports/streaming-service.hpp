#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/streaming/models/app-stream-config.hpp"
#include "domain/streaming/models/platform-stream-config.hpp"

namespace sst::streaming {

struct ActivePlatformStream {
    std::int64_t stream_id{0};
    std::string name;
};

// Sole entry point for the BLE controller and the orchestration thread.
// Implementations also expose the IFrameSink interface — the orchestration
// thread calls Push() on every post-processed final frame, and the service
// fans out to the always-on app stream + every active platform stream.
class IStreamingService {
   public:
    virtual ~IStreamingService() = default;

    virtual auto StartAppStream(const AppStreamConfig& config) -> bool = 0;
    virtual auto StopAppStream() -> bool = 0;
    [[nodiscard]] virtual auto IsAppStreamRunning() const -> bool = 0;

    virtual auto StartPlatformStream(const PlatformStreamConfig& config) -> bool = 0;
    virtual auto StopPlatformStream(std::int64_t stream_id) -> bool = 0;

    [[nodiscard]] virtual auto ListActivePlatformStreams() const
        -> std::vector<ActivePlatformStream> = 0;
};

}  // namespace sst::streaming
