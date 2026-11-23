#include "adapters/capture/frame/gstreamer/gstreamer.hpp"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstallocator.h>
#include <gst/gstmemory.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "app/config/services/config_loader/config-loader.hpp"
#include "domain/capture/models/camera-config.hpp"
#include "domain/capture/models/video-surface.hpp"
#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

namespace fs = std::filesystem;
constexpr std::size_t kSyntheticBytes = 16;

namespace {

constexpr const char* kConfigDir = SST_REPO_ROOT_DIR "/tests/config/config_files";
constexpr GstClockTime kEarlyPts = 10 * GST_SECOND;
constexpr GstClockTime kLatePts = 5 * GST_SECOND;
constexpr GstClockTime kFirstBase = 100 * GST_SECOND;
constexpr GstClockTime kRestartBase = 200 * GST_SECOND;

struct SyntheticNvmmMemory {
    GstMemory memory;
};

struct SyntheticAllocator {
    GstAllocator parent;
};

struct SyntheticAllocatorClass {
    GstAllocatorClass parent;
};

G_DEFINE_TYPE(SyntheticAllocator, synthetic_allocator, GST_TYPE_ALLOCATOR)

auto FreeSyntheticNvmm(GstAllocator* unused, GstMemory* memory) -> void {
    static_cast<void>(unused);
    delete reinterpret_cast<SyntheticNvmmMemory*>(memory);
}

auto synthetic_allocator_class_init(SyntheticAllocatorClass* klass) -> void {
    GST_ALLOCATOR_CLASS(klass)->free = FreeSyntheticNvmm;
}

auto synthetic_allocator_init(SyntheticAllocator* allocator) -> void {
    GST_ALLOCATOR(allocator)->mem_type = "NVMM";
}

auto SyntheticNvmmAllocator() -> GstAllocator* {
    static auto* allocator =
        static_cast<GstAllocator*>(g_object_new(synthetic_allocator_get_type(), nullptr));
    return allocator;
}

auto MakeNvmmSample(GstClockTime pts,
                    GstBufferFlags flags = static_cast<GstBufferFlags>(0)) -> GstSample* {
    auto* allocator = SyntheticNvmmAllocator();
    auto* memory = new SyntheticNvmmMemory{};
    gst_memory_init(&memory->memory, GST_MEMORY_FLAG_NO_SHARE, allocator, nullptr, kSyntheticBytes,
                    0, 0, kSyntheticBytes);
    GstBuffer* buffer = gst_buffer_new();
    gst_buffer_append_memory(buffer, &memory->memory);
    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_FLAG_SET(buffer, flags);
    GstCaps* caps = gst_caps_from_string(
        "video/x-raw(memory:NVMM),format=NV12,width=3840,height=2160,framerate=30/1");
    GstSample* sample = gst_sample_new(buffer, caps, nullptr, nullptr);
    gst_buffer_unref(buffer);
    gst_caps_unref(caps);
    return sample;
}

}  // namespace

TEST(GstreamerAdapter, DefaultPipelineUses60HzAntibandingForBothSensors) {
    const char* previous = ::getenv("SST_ISP_TUNING");
    const std::string saved = previous == nullptr ? "" : previous;
    ASSERT_EQ(::unsetenv("SST_ISP_TUNING"), 0);
    for (int sensor_id : {0, 1}) {
        for (bool nvmm : {false, true}) {
            sst::capture::GStreamerAdapter adapter({}, "v1", static_cast<std::uint16_t>(sensor_id),
                                                   nvmm);
            const std::string pipeline = adapter.CreatePipeline();
            EXPECT_NE(pipeline.find("nvarguscamerasrc sensor-id=" + std::to_string(sensor_id) +
                                    " tnr-mode=2 tnr-strength=0.9 ee-mode=1 ee-strength=0.4 "
                                    "aeantibanding=3 !"),
                      std::string::npos);
        }
    }
    ASSERT_EQ(::setenv("SST_ISP_TUNING", "tnr-mode=1", 1), 0);
    sst::capture::GStreamerAdapter override_adapter({}, "v1", 0);
    const std::string override_pipeline = override_adapter.CreatePipeline();
    EXPECT_NE(override_pipeline.find("sensor-id=0 tnr-mode=1 !"), std::string::npos);
    EXPECT_EQ(override_pipeline.find("aeantibanding=3"), std::string::npos);
    if (previous == nullptr) {
        EXPECT_EQ(::unsetenv("SST_ISP_TUNING"), 0);
    } else {
        EXPECT_EQ(::setenv("SST_ISP_TUNING", saved.c_str(), 1), 0);
    }
}

TEST(GstreamerAdapter, NvmmModeNormalizesPtsAcrossCamerasAndRetainsBuffer) {
    sst::capture::GStreamerAdapter cam0({}, "SST-CAM-v1", 0, true);
    sst::capture::GStreamerAdapter cam1({}, "SST-CAM-v1", 1, true);
    GstClock* clock = gst_system_clock_obtain();
    constexpr GstClockTime kBase0 = kFirstBase;
    constexpr GstClockTime kBase1 = kFirstBase + kLatePts;
    GstSample* first = MakeNvmmSample(kEarlyPts);
    GstSample* second = MakeNvmmSample(kLatePts + GST_MSECOND);
    auto left = cam0.SurfaceFromSample(first, clock, kBase0);
    auto right = cam1.SurfaceFromSample(second, clock, kBase1);
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    EXPECT_EQ(left.value_or(sst::capture::VideoSurface{}).raw_pts, std::chrono::seconds(10));
    EXPECT_EQ(left.value_or(sst::capture::VideoSurface{}).capture_pts, std::chrono::seconds(110));
    EXPECT_EQ(right.value_or(sst::capture::VideoSurface{}).capture_pts,
              std::chrono::seconds(110) + std::chrono::milliseconds(1));
    EXPECT_EQ(left.value_or(sst::capture::VideoSurface{}).clock_epoch,
              right.value_or(sst::capture::VideoSurface{}).clock_epoch);
    EXPECT_EQ(left.value_or(sst::capture::VideoSurface{}).clock_epoch,
              static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(clock)));
    gst_sample_unref(first);
    gst_sample_unref(second);
    EXPECT_TRUE(left.value_or(sst::capture::VideoSurface{}).owner);
    EXPECT_TRUE(right.value_or(sst::capture::VideoSurface{}).owner);
    gst_object_unref(clock);
}

TEST(GstreamerAdapter, NvmmModeRejectsInvalidPtsAndDiscontinuousSamples) {
    sst::capture::GStreamerAdapter adapter({}, "SST-CAM-v1", 0, true);
    GstClock* clock = gst_system_clock_obtain();
    GstSample* invalid = MakeNvmmSample(GST_CLOCK_TIME_NONE);
    EXPECT_FALSE(adapter.SurfaceFromSample(invalid, clock, GST_SECOND));
    GstSample* discontinuous = MakeNvmmSample(GST_SECOND, GST_BUFFER_FLAG_DISCONT);
    auto surface = adapter.SurfaceFromSample(discontinuous, clock, GST_SECOND);
    ASSERT_TRUE(surface.has_value());
    EXPECT_TRUE(surface.value_or(sst::capture::VideoSurface{}).discontinuity);
    gst_sample_unref(invalid);
    gst_sample_unref(discontinuous);
    gst_object_unref(clock);
}

TEST(GstreamerAdapter, NvmmModeRefreshesBaseTimeOnRestartAndRejectsRegressedPts) {
    sst::capture::GStreamerAdapter adapter({}, "SST-CAM-v1", 0, true);
    GstClock* clock = gst_system_clock_obtain();
    GstSample* first = MakeNvmmSample(GST_SECOND);
    auto before = adapter.SurfaceFromSample(first, clock, kFirstBase);
    ASSERT_TRUE(before.has_value());
    EXPECT_TRUE(before.value_or(sst::capture::VideoSurface{}).discontinuity);
    EXPECT_FALSE(adapter.SurfaceFromSample(first, clock, kFirstBase));
    GstSample* resumed = MakeNvmmSample(2 * GST_SECOND);
    auto after = adapter.SurfaceFromSample(resumed, clock, kFirstBase);
    ASSERT_TRUE(after.has_value());
    EXPECT_TRUE(after.value_or(sst::capture::VideoSurface{}).discontinuity);
    GstSample* restarted = MakeNvmmSample(GST_SECOND);
    auto fresh = adapter.SurfaceFromSample(restarted, clock, kRestartBase);
    ASSERT_TRUE(fresh.has_value());
    EXPECT_EQ(fresh.value_or(sst::capture::VideoSurface{}).capture_pts, std::chrono::seconds(201));
    EXPECT_EQ(fresh.value_or(sst::capture::VideoSurface{}).clock_epoch,
              before.value_or(sst::capture::VideoSurface{}).clock_epoch);
    EXPECT_TRUE(fresh.value_or(sst::capture::VideoSurface{}).discontinuity);
    gst_sample_unref(first);
    gst_sample_unref(resumed);
    gst_sample_unref(restarted);
    gst_object_unref(clock);
}

TEST(GstreamerAdapter, NvmmModeRequiresCommonClockAndNvmmBuffer) {
    sst::capture::GStreamerAdapter adapter({}, "SST-CAM-v1", 0, true);
    GstCaps* caps = gst_caps_from_string(
        "video/x-raw(memory:NVMM),format=NV12,width=3840,height=2160,framerate=30/1");
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, kSyntheticBytes, nullptr);
    GstSample* sample = gst_sample_new(buffer, caps, nullptr, nullptr);
    gst_buffer_unref(buffer);
    gst_caps_unref(caps);
    ASSERT_NE(sample, nullptr);
    GstClock* clock = gst_system_clock_obtain();
    EXPECT_FALSE(adapter.SurfaceFromSample(sample, clock, GST_SECOND));
    gst_sample_unref(sample);

    GstCaps* system_caps =
        gst_caps_from_string("video/x-raw,format=NV12,width=3840,height=2160,framerate=30/1");
    GstBuffer* system_buffer = gst_buffer_new_allocate(nullptr, kSyntheticBytes, nullptr);
    GstCaps* reference = gst_caps_from_string("timestamp/x-sst-common-monotonic");
    gst_buffer_add_reference_timestamp_meta(system_buffer, reference, GST_SECOND,
                                            GST_CLOCK_TIME_NONE);
    GstSample* system_sample = gst_sample_new(system_buffer, system_caps, nullptr, nullptr);
    EXPECT_FALSE(adapter.SurfaceFromSample(system_sample, clock, GST_SECOND));
    gst_sample_unref(system_sample);
    gst_buffer_unref(system_buffer);
    gst_caps_unref(system_caps);

    GstCaps* nvmm_caps = gst_caps_from_string(
        "video/x-raw(memory:NVMM),format=NV12,width=3840,height=2160,framerate=30/1");
    GstBuffer* fake_nvmm = gst_buffer_new_allocate(nullptr, kSyntheticBytes, nullptr);
    gst_buffer_add_reference_timestamp_meta(fake_nvmm, reference, GST_SECOND, GST_CLOCK_TIME_NONE);
    GstSample* fake_sample = gst_sample_new(fake_nvmm, nvmm_caps, nullptr, nullptr);
    EXPECT_FALSE(adapter.SurfaceFromSample(fake_sample, clock, GST_SECOND));
    GstSample* valid = MakeNvmmSample(GST_SECOND);
    EXPECT_FALSE(adapter.SurfaceFromSample(valid, nullptr, GST_SECOND));
    EXPECT_FALSE(adapter.SurfaceFromSample(valid, clock, GST_CLOCK_TIME_NONE));
    gst_sample_unref(valid);
    gst_object_unref(clock);
    gst_sample_unref(fake_sample);
    gst_buffer_unref(fake_nvmm);
    gst_caps_unref(nvmm_caps);
    gst_caps_unref(reference);
}

TEST(GstreamerAdapter, TwoNvmmCamerasShareVerifiedMonotonicTimestampForThirtyMinutes) {
    sst::capture::GStreamerAdapter cam0({}, "SST-CAM-v1", 0, true);
    sst::capture::GStreamerAdapter cam1({}, "SST-CAM-v1", 1, true);
    cam0.Start();
    cam1.Start();
    ASSERT_TRUE(cam0.IsRunning());
    ASSERT_TRUE(cam1.IsRunning());
    sst::pipeline::FramePairSynchronizer sync;
    std::vector<std::chrono::nanoseconds> skews;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(30);
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto* camera : {&cam0, &cam1}) {
            auto surface = camera->CaptureSurface();
            ASSERT_TRUE(camera->IsRunning());
            if (!surface) {
                continue;
            }
            EXPECT_EQ(surface->geometry.width, 3840U);
            EXPECT_EQ(surface->geometry.height, 2160U);
            EXPECT_TRUE(surface->owner);
            if (auto pair = sync.Push(surface->camera_index, std::move(*surface))) {
                skews.push_back(pair->skew);
            }
        }
    }
    cam0.Stop();
    cam1.Stop();
    ASSERT_FALSE(skews.empty());
    std::sort(skews.begin(), skews.end());
    const auto p99 = skews[(skews.size() - 1) * 99 / 100];
    EXPECT_LE(p99, std::chrono::milliseconds(19));
    EXPECT_TRUE(sync.Synchronized());
}

// Capture test runs against real Jetson hardware (IMX477 via nvarguscamerasrc).
// Build in dev container, scp the test binary to the Jetson, run with:
//   ./sst_cam_firmware_tests --gtest_filter=GstreamerAdapter.*
TEST(GstreamerAdapter, CaptureSingleFrameAndLog) {
    sst::config::app::ConfigLoader loader(kConfigDir, "json");
    sst::config::ConfigData cfg = loader.get();
    const std::string device_model = cfg.device.model.value_or("");

    // Camera sensor config is a compiled-in default now (app-as-source-of-truth:
    // no local DB). Lens calibration still comes from calibration.json.
    const sst::capture::CameraConfig camera_cfg{};

    constexpr std::uint16_t kCameraIndex = 0;
    sst::capture::GStreamerAdapter adapter(camera_cfg, device_model, kCameraIndex);
    adapter.Start();
    ASSERT_TRUE(adapter.IsRunning()) << "GStreamer pipeline did not start";

    std::optional<sst::capture::Frame> frame;
    std::optional<sst::capture::Frame> prev;

    constexpr int kMaxSeconds = 5;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kMaxSeconds);

    spdlog::info("Starting frame capture loop...");

    while (std::chrono::steady_clock::now() < deadline) {
        auto captured = adapter.Capture();
        if (!captured) {
            std::this_thread::yield();
            continue;
        }

        frame = *captured;

        if (prev) {
            auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                frame->captured_at - prev->captured_at)
                                .count();
            if (delta_ms > 0) {
                constexpr double kMillisecondsPerSecond = 1000.0;
                double fps = kMillisecondsPerSecond / static_cast<double>(delta_ms);
                spdlog::info("frame_id={} dt={} ms (~{:.1f} fps)", frame->frame_id, delta_ms, fps);
            }
        }

        prev = *frame;
    }

    spdlog::info("Frame capture loop ended.");

    if (!frame) {
        FAIL() << "Failed to capture frame from GStreamer";
        return;
    }

    const auto ts_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(frame->captured_at.time_since_epoch())
            .count();

    spdlog::info("Captured frame:");
    spdlog::info("  frame_id      = {}", frame->frame_id);
    spdlog::info("  timestamp_ms  = {}", ts_ms);
    spdlog::info("  width         = {}", frame->geometry.width);
    spdlog::info("  height        = {}", frame->geometry.height);
    spdlog::info("  planes        = {}", frame->planes.size());
    spdlog::info("  bytes         = {}", frame->planes[0].size);
    spdlog::info("  stride        = {}", frame->planes[0].stride);
    spdlog::info("  pixel_format  = {}", static_cast<int>(frame->format));

    adapter.Stop();
    EXPECT_FALSE(adapter.IsRunning());
}
