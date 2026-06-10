#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/storage/ports/raw-capture-sink.hpp"
#include "domain/buffer/services/drop-oldest-ring.hpp"
#include "domain/capture/models/frame.hpp"

namespace sst::adapters::storage {

// Filesystem raw dual-camera sink. One file per camera under `video_dir`, named
// raw__<group>__cam<N>.nv12 (see domain/storage/services/raw-capture-naming.hpp),
// holding concatenated NV12 frames. Each camera has its own bounded drop-oldest
// queue + writer thread, so PushCamera never blocks the capture thread and a
// slow disk drops frames rather than stalling capture.
//
// Raw NV12 is chosen over re-encoding to avoid two more software encodes on top
// of the recording/preview/RTMP encodes (the Orin Nano has no NVENC and the CPU
// budget is tight) and to keep the most faithful pixels for downstream YOLO
// training. The trade is disk bandwidth (~93 MB/s per camera at 1080p30).
class FilesystemRawCaptureSink final : public sst::storage::IRawCaptureSink {
   public:
    FilesystemRawCaptureSink(std::filesystem::path video_dir, std::uint32_t camera_count);
    ~FilesystemRawCaptureSink() override;

    FilesystemRawCaptureSink(const FilesystemRawCaptureSink&) = delete;
    auto operator=(const FilesystemRawCaptureSink&) -> FilesystemRawCaptureSink& = delete;
    FilesystemRawCaptureSink(FilesystemRawCaptureSink&&) = delete;
    auto operator=(FilesystemRawCaptureSink&&) -> FilesystemRawCaptureSink& = delete;

    auto Start(const std::string& capture_group_id) -> bool override;
    auto PushCamera(std::uint32_t camera_index, const sst::capture::Frame& frame) -> void override;
    auto Stop() -> bool override;
    [[nodiscard]] auto IsCapturing() const -> bool override;

   private:
    // Per-camera writer: a bounded drop-oldest queue drained by a dedicated
    // thread into one open file. Non-movable (atomic + thread + ring), so the
    // sink owns these by unique_ptr.
    struct CameraWriter {
        std::ofstream file;
        sst::buffer::DropOldestRing<sst::capture::Frame> ring;
        std::thread thread;
        std::atomic<bool> stopping{false};

        explicit CameraWriter(std::size_t queue_capacity) : ring(queue_capacity) {}
    };

    static auto WriterLoop(CameraWriter* writer) -> void;

    const std::filesystem::path video_dir_;
    const std::uint32_t camera_count_;

    mutable std::mutex mtx_;
    std::atomic<bool> capturing_{false};
    std::string capture_group_id_;
    std::vector<std::unique_ptr<CameraWriter>> writers_;
};

}  // namespace sst::adapters::storage
