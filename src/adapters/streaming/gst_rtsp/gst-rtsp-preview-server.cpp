#include "adapters/streaming/gst_rtsp/gst-rtsp-preview-server.hpp"

#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "domain/streaming/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::adapters::streaming {

namespace {

auto BuildLaunchString(const sst::streaming::PreviewConfig& cfg) -> std::string {
    // Sensor capture stays at the sensor's native 1080p30 — downscale happens
    // in the NVMM domain via nvvidconv, then NVENC encodes at the requested
    // bitrate. iframe-interval == framerate => one keyframe per second, which
    // keeps RTSP join latency under ~1s.
    return fmt::format(
        "( nvarguscamerasrc sensor-id=0 "
        "  ! video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1,format=NV12 "
        "  ! nvvidconv "
        "  ! video/x-raw(memory:NVMM),width={w},height={h} "
        "  ! nvv4l2h264enc bitrate={bitrate} insert-sps-pps=true iframeinterval={fps} "
        "  ! h264parse "
        "  ! rtph264pay name=pay0 pt=96 config-interval=1 )",
        fmt::arg("w", cfg.width), fmt::arg("h", cfg.height),
        fmt::arg("bitrate", cfg.bitrate_kbps * 1000), fmt::arg("fps", cfg.framerate));
}

}  // namespace

GstRtspPreviewServer::GstRtspPreviewServer() { gst_init(nullptr, nullptr); }

GstRtspPreviewServer::~GstRtspPreviewServer() {
    if (running_) {
        Stop();
    }
}

auto GstRtspPreviewServer::Start(const sst::streaming::PreviewConfig& config) -> bool {
    std::lock_guard lock(mtx_);
    if (running_) {
        spdlog::warn("GstRtspPreviewServer::Start: already running");
        return true;
    }

    spdlog::info("GstRtspPreviewServer::Start {}", config);

    context_ = g_main_context_new();
    loop_ = g_main_loop_new(context_, FALSE);

    g_main_context_push_thread_default(context_);

    server_ = gst_rtsp_server_new();
    const std::string port_str = std::to_string(config.port);
    gst_rtsp_server_set_service(server_, port_str.c_str());

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server_);
    GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();

    const std::string launch = BuildLaunchString(config);
    spdlog::info("GstRtspPreviewServer: launch = {}", launch);
    gst_rtsp_media_factory_set_launch(factory, launch.c_str());
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(mounts, config.mount_point.c_str(), factory);
    g_object_unref(mounts);

    source_id_ = gst_rtsp_server_attach(server_, context_);
    g_main_context_pop_thread_default(context_);

    if (source_id_ == 0) {
        spdlog::error("GstRtspPreviewServer: gst_rtsp_server_attach failed");
        g_object_unref(server_);
        server_ = nullptr;
        g_main_loop_unref(loop_);
        loop_ = nullptr;
        g_main_context_unref(context_);
        context_ = nullptr;
        return false;
    }

    running_ = true;
    loop_thread_ = std::thread([this] {
        g_main_context_push_thread_default(context_);
        g_main_loop_run(loop_);
        g_main_context_pop_thread_default(context_);
    });

    spdlog::info("GstRtspPreviewServer: serving rtsp://0.0.0.0:{}{}", config.port,
                 config.mount_point);
    return true;
}

auto GstRtspPreviewServer::Stop() -> void {
    std::thread to_join;
    {
        std::lock_guard lock(mtx_);
        if (!running_) {
            return;
        }
        spdlog::info("GstRtspPreviewServer::Stop");

        if (loop_ != nullptr && g_main_loop_is_running(loop_) != 0) {
            g_main_loop_quit(loop_);
        }
        to_join = std::move(loop_thread_);
        running_ = false;
    }

    if (to_join.joinable()) {
        to_join.join();
    }

    std::lock_guard lock(mtx_);
    if (source_id_ != 0 && context_ != nullptr) {
        GSource* src = g_main_context_find_source_by_id(context_, source_id_);
        if (src != nullptr) {
            g_source_destroy(src);
        }
        source_id_ = 0;
    }
    if (server_ != nullptr) {
        g_object_unref(server_);
        server_ = nullptr;
    }
    if (loop_ != nullptr) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    if (context_ != nullptr) {
        g_main_context_unref(context_);
        context_ = nullptr;
    }
}

auto GstRtspPreviewServer::IsRunning() const -> bool { return running_; }

}  // namespace sst::adapters::streaming
