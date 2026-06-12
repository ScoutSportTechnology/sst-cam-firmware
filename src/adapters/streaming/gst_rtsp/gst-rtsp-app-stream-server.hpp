#pragma once

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "app/streaming/ports/app-stream-server.hpp"

namespace sst::adapters::streaming {

// RTSP server fed by an appsrc. The companion app connects over WiFi-Direct,
// pulls the stream from `mount_point` on `port`, and sees the post-processed
// frames the device pushes via Push().
//
// Pipeline (per RTSP client, instantiated by gst-rtsp-server when the client
// connects):
//
//   appsrc name=src is-live=true format=time
//     ! videoconvert ! nvvidconv
//     ! "video/x-raw(memory:NVMM),format=NV12"
//     ! nvv4l2h264enc bitrate=B insert-sps-pps=true iframeinterval=fps
//     ! h264parse ! rtph264pay name=pay0 pt=96 config-interval=1
//
// The appsrc is shared across all RTSP sessions via gst-rtsp-server's
// `shared=true` factory mode — every connected client sees the same encoded
// stream, which means one NVENC session regardless of viewer count.
//
// Lifecycle: Start() spins a dedicated GMainLoop thread; Stop() quits it.
// The underlying appsrc is acquired via `media-configure` — once a client
// connects we cache the pointer so Push() can route into it. Push() calls
// before any client connects are silently dropped.
class GstRtspAppStreamServer final : public sst::streaming::IAppStreamServer {
   public:
    GstRtspAppStreamServer();
    ~GstRtspAppStreamServer() override;

    GstRtspAppStreamServer(const GstRtspAppStreamServer&) = delete;
    auto operator=(const GstRtspAppStreamServer&) -> GstRtspAppStreamServer& = delete;
    GstRtspAppStreamServer(GstRtspAppStreamServer&&) = delete;
    auto operator=(GstRtspAppStreamServer&&) -> GstRtspAppStreamServer& = delete;

    auto Start(const sst::streaming::AppStreamConfig& config) -> bool override;
    auto Stop() -> void override;
    [[nodiscard]] auto IsRunning() const -> bool override;

    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    static auto OnMediaConfigureStatic(GstRTSPMediaFactory* factory, GstRTSPMedia* media,
                                       gpointer user_data) -> void;
    auto OnMediaConfigure(GstRTSPMedia* media) -> void;

    sst::streaming::AppStreamConfig config_{};

    mutable std::mutex mtx_;
    std::atomic<bool> running_{false};

    GMainContext* context_{nullptr};
    GMainLoop* loop_{nullptr};
    GstRTSPServer* server_{nullptr};
    GstRTSPMediaFactory* factory_{nullptr};
    guint source_id_{0};
    std::thread loop_thread_;

    // Bound when an RTSP client connects (via media-configure). nullptr until
    // then — Push() drops frames silently in that window.
    GstAppSrc* appsrc_{nullptr};
};

}  // namespace sst::adapters::streaming
