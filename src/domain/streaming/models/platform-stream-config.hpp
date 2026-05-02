#pragma once

#include <cstdint>
#include <string>

namespace sst::streaming {

enum class PlatformStreamType : std::uint8_t { kRtmp = 0, kRtmpS = 1 };
enum class PlatformStreamCodec : std::uint8_t { kH264 = 0, kH265 = 1 };

// In-flight value object passed to a per-destination streamer adapter. The
// streaming service builds this from a sst::db::StreamConfig row at start
// time, so user edits to the DB row take effect on the next StartPlatformStream
// call. Decouples the streaming module from the DB types.
struct PlatformStreamConfig {
    int64_t stream_id{0};
    std::string name;
    PlatformStreamType type{PlatformStreamType::kRtmp};
    std::string url;
    std::string stream_key;
    PlatformStreamCodec codec{PlatformStreamCodec::kH264};
    std::int32_t width{1920};
    std::int32_t height{1080};
    std::int32_t framerate{60};
    std::int32_t bitrate_kbps{4000};
};

}  // namespace sst::streaming
