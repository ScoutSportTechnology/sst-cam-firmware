#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "app/streaming/ports/preview-server.hpp"

namespace sst::adapters::streaming {

// RTSP preview server. Builds a self-contained NVENC pipeline (nvarguscamerasrc
// → nvvidconv downscale → nvv4l2h264enc → rtph264pay) and serves it under the
// configured mount point on the configured port. Uses an isolated GMainContext
// + GMainLoop on a dedicated thread so it doesn't interfere with the capture
// pipeline's bus polling on the main thread.
class GstRtspPreviewServer final : public sst::streaming::IPreviewServer {
   public:
    GstRtspPreviewServer();
    ~GstRtspPreviewServer() override;

    GstRtspPreviewServer(const GstRtspPreviewServer&) = delete;
    auto operator=(const GstRtspPreviewServer&) -> GstRtspPreviewServer& = delete;
    GstRtspPreviewServer(GstRtspPreviewServer&&) = delete;
    auto operator=(GstRtspPreviewServer&&) -> GstRtspPreviewServer& = delete;

    auto Start(const sst::streaming::PreviewConfig& config) -> bool override;
    auto Stop() -> void override;
    [[nodiscard]] auto IsRunning() const -> bool override;

   private:
    mutable std::mutex mtx_;
    GMainContext* context_{nullptr};
    GMainLoop* loop_{nullptr};
    GstRTSPServer* server_{nullptr};
    guint source_id_{0};
    std::thread loop_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace sst::adapters::streaming
