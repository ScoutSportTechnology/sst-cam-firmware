#pragma once

#include <cstdint>
#include <string>

namespace sst::db {

enum class StreamPlatform : uint8_t {
    kYouTube = 0,
    kTwitch = 1,
    kFacebook = 2,
    kInstagram = 3,
    kTikTok = 4,
    kCustom = 5
};

enum class StreamType : uint8_t { kRtmp = 0, kRtmps = 1 };
enum class StreamCodec : uint8_t { kH264 = 0, kH265 = 1 };

struct StreamConfig {
    static constexpr int32_t kDefaultWidth = 1920;
    static constexpr int32_t kDefaultHeight = 1080;
    static constexpr int32_t kDefaultFramerate = 60;
    static constexpr int32_t kDefaultBitrateKbps = 4000;

    int64_t id{0};
    int64_t user_id{0};
    StreamPlatform platform{StreamPlatform::kCustom};
    std::string name;
    bool enabled{false};
    std::string stream_key;
    StreamType stream_type{StreamType::kRtmp};
    std::string url;
    StreamCodec codec{StreamCodec::kH264};
    int32_t width{kDefaultWidth};
    int32_t height{kDefaultHeight};
    int32_t framerate{kDefaultFramerate};
    int32_t bitrate_kbps{kDefaultBitrateKbps};
};

}  // namespace sst::db
