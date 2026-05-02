#pragma once

#include <cstdint>
#include <string>

namespace sst::streaming {

// Always-on, low-res RTSP stream the companion app picks up over WiFi-Direct.
// Consumed via appsrc by GstRtspAppStreamServer — the device pushes
// post-processed frames (BGR8) into the server's appsrc, the server encodes
// with NVENC and serves under `mount_point` on `port`.
struct AppStreamConfig {
    static constexpr std::uint32_t kDefaultWidth = 854;
    static constexpr std::uint32_t kDefaultHeight = 480;
    static constexpr std::uint32_t kDefaultFramerate = 30;
    static constexpr std::uint32_t kDefaultBitrateKbps = 1500;
    static constexpr std::uint16_t kDefaultPort = 8554;

    std::string mount_point{"/preview"};
    std::uint16_t port{kDefaultPort};
    std::uint32_t width{kDefaultWidth};
    std::uint32_t height{kDefaultHeight};
    std::uint32_t framerate{kDefaultFramerate};
    std::uint32_t bitrate_kbps{kDefaultBitrateKbps};
};

}  // namespace sst::streaming
