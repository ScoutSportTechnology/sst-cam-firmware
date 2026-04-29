#pragma once

#include <cstdint>
#include <string>

namespace sst::streaming {

struct PreviewConfig {
    std::string mount_point{"/preview"};
    std::uint16_t port{8554};
    std::uint32_t width{854};
    std::uint32_t height{480};
    std::uint32_t framerate{30};
    std::uint32_t bitrate_kbps{1500};
};

}  // namespace sst::streaming
