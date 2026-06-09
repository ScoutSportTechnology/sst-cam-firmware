#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

#include <gst/gst.h>

#include "app/streaming/ports/platform-streamer.hpp"

namespace sst::adapters::streaming {

// Per-platform RTMP/RTMPS push pipeline:
//
//   appsrc → videoconvert → nvvidconv
//          → "video/x-raw(memory:NVMM),format=NV12,width=W,height=H,framerate=N/1"
//          → nvv4l2h264enc bitrate=B insert-sps-pps=true iframeinterval=N
//          → h264parse → flvmux → rtmpsink location=<url>/<key>
//
// rtmpsink ships in `gstreamer1.0-plugins-bad`. Same element handles both
// rtmp:// and rtmps:// — the `location` URL string carries the scheme.
class GstRtmpStreamer final : public sst::streaming::IPlatformStreamer {
   public:
    GstRtmpStreamer();
    ~GstRtmpStreamer() override;

    GstRtmpStreamer(const GstRtmpStreamer&) = delete;
    auto operator=(const GstRtmpStreamer&) -> GstRtmpStreamer& = delete;
    GstRtmpStreamer(GstRtmpStreamer&&) = delete;
    auto operator=(GstRtmpStreamer&&) -> GstRtmpStreamer& = delete;

    auto Start(const sst::streaming::PlatformStreamConfig& config) -> bool override;
    auto Stop() -> void override;
    [[nodiscard]] auto IsRunning() const -> bool override;
    [[nodiscard]] auto Id() const -> std::int64_t override;

    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    auto Teardown() -> void;

    sst::streaming::PlatformStreamConfig config_{};

    mutable std::mutex mtx_;
    std::atomic<bool> running_{false};

    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
};

}  // namespace sst::adapters::streaming
