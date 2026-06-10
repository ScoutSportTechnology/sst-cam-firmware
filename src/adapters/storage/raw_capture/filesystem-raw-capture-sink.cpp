#include "adapters/storage/raw_capture/filesystem-raw-capture-sink.hpp"

#include <chrono>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

#include "domain/storage/models/raw-capture-identity.hpp"
#include "domain/storage/services/raw-capture-naming.hpp"

namespace sst::adapters::storage {

namespace {
// Per-camera queue depth. At ~93 MB/s a 1080p30 NV12 frame is ~3 MB; 8 frames
// (~24 MB, ~0.25 s) absorbs a brief disk hiccup before dropping oldest.
constexpr std::size_t kQueueCapacity = 8;
constexpr std::chrono::milliseconds kPopTimeout{100};

auto WriteFramePlanes(std::ofstream& file, const sst::capture::Frame& frame) -> void {
    for (const auto& plane : frame.planes) {
        if (plane.data != nullptr && plane.size > 0) {
            file.write(reinterpret_cast<const char*>(plane.data),
                       static_cast<std::streamsize>(plane.size));
        }
    }
}
}  // namespace

FilesystemRawCaptureSink::FilesystemRawCaptureSink(std::filesystem::path video_dir,
                                                   std::uint32_t camera_count)
    : video_dir_(std::move(video_dir)), camera_count_(camera_count) {}

FilesystemRawCaptureSink::~FilesystemRawCaptureSink() {
    if (capturing_.load()) {
        (void)Stop();
    }
}

auto FilesystemRawCaptureSink::Start(const std::string& capture_group_id) -> bool {
    std::lock_guard lock(mtx_);
    if (capturing_.load()) {
        spdlog::warn("RawCaptureSink::Start: already capturing");
        return false;
    }
    if (capture_group_id.empty()) {
        spdlog::error("RawCaptureSink::Start: empty capture_group_id");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(video_dir_, ec);
    if (ec) {
        spdlog::error("RawCaptureSink::Start: cannot create {}: {}", video_dir_.string(),
                      ec.message());
        return false;
    }

    std::vector<std::unique_ptr<CameraWriter>> writers;
    writers.reserve(camera_count_);
    for (std::uint32_t i = 0; i < camera_count_; ++i) {
        auto writer = std::make_unique<CameraWriter>(kQueueCapacity);
        const auto name = sst::storage::raw_capture_naming::FileName(
            sst::storage::RawCaptureIdentity{.capture_group_id = capture_group_id,
                                             .camera_index = i});
        const auto path = video_dir_ / name;
        writer->file.open(path, std::ios::binary | std::ios::trunc);
        if (!writer->file.is_open()) {
            spdlog::error("RawCaptureSink::Start: cannot open {}", path.string());
            return false;  // writers (and their files) roll back as the vector dies
        }
        writers.push_back(std::move(writer));
    }

    // Only commit (spawn threads, flip state) once every file opened cleanly.
    for (auto& writer : writers) {
        writer->thread = std::thread(&FilesystemRawCaptureSink::WriterLoop, writer.get());
    }
    writers_ = std::move(writers);
    capture_group_id_ = capture_group_id;
    capturing_.store(true);
    spdlog::info("RawCaptureSink: started group={} cameras={}", capture_group_id, camera_count_);
    return true;
}

auto FilesystemRawCaptureSink::PushCamera(std::uint32_t camera_index,
                                          const sst::capture::Frame& frame) -> void {
    if (!capturing_.load() || camera_index >= writers_.size()) {
        return;
    }
    // Copy shares the frame's owner shared_ptr (no pixel copy); the ring keeps
    // the bytes alive until the writer thread drains them. Non-blocking: the
    // ring drops its oldest frame instead of stalling this (capture) thread.
    writers_[camera_index]->ring.Push(frame);
}

auto FilesystemRawCaptureSink::Stop() -> bool {
    std::lock_guard lock(mtx_);
    if (!capturing_.load()) {
        return false;
    }
    capturing_.store(false);  // PushCamera no-ops from here

    for (auto& writer : writers_) {
        writer->stopping.store(true);
        writer->ring.Close();  // wake the blocked Pop
        if (writer->thread.joinable()) {
            writer->thread.join();
        }
        // Drain anything left after the thread exited (single-threaded now).
        while (auto frame = writer->ring.TryPop()) {
            WriteFramePlanes(writer->file, *frame);
        }
        writer->file.flush();
        writer->file.close();
    }
    spdlog::info("RawCaptureSink: stopped group={}", capture_group_id_);
    writers_.clear();
    capture_group_id_.clear();
    return true;
}

auto FilesystemRawCaptureSink::IsCapturing() const -> bool { return capturing_.load(); }

auto FilesystemRawCaptureSink::WriterLoop(CameraWriter* writer) -> void {
    while (!writer->stopping.load()) {
        auto frame = writer->ring.Pop(kPopTimeout);
        if (frame) {
            WriteFramePlanes(writer->file, *frame);
        }
    }
    // Drain whatever arrived before stopping was observed.
    while (auto frame = writer->ring.TryPop()) {
        WriteFramePlanes(writer->file, *frame);
    }
}

}  // namespace sst::adapters::storage
