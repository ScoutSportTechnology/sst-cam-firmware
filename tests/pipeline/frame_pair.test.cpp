#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>

#include "domain/capture/models/camera-config.hpp"
#include "domain/capture/models/video-surface.hpp"
#include "domain/pipeline/services/frame-pair-ring.hpp"
#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

namespace {
using namespace std::chrono_literals;
using sst::capture::VideoSurface;
using sst::pipeline::FramePairRing;
using sst::pipeline::FramePairSynchronizer;

constexpr int kFps = 30;
constexpr std::size_t kSurfaceBytes = 10;
constexpr std::size_t kPairBytes = 2 * kSurfaceBytes;
constexpr int kInitialMs = 100;
constexpr int kIntervalMs = 34;
constexpr std::size_t kFallbackPairs = 30;
constexpr std::size_t kFullPairs = 45;

auto Surface(std::uint16_t camera, std::chrono::nanoseconds pts, std::size_t bytes = kSurfaceBytes,
             std::uint64_t epoch = 1) -> VideoSurface {
    return {.owner = std::make_shared<int>(1),
            .producer_fence = {},
            .camera_index = camera,
            .capture_pts = pts,
            .raw_pts = pts,
            .clock_epoch = epoch,
            .geometry = {.width = sst::capture::CameraConfig::kDefaultSensorWidth,
                         .height = sst::capture::CameraConfig::kDefaultSensorHeight},
            .bytes = bytes};
}

TEST(FramePair, RestartWithNewEpochWaitsForBothNewEpochFrames) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms, kSurfaceBytes, 1)));
    auto initial = sync.Push(1, Surface(1, 100ms, kSurfaceBytes, 1));
    EXPECT_TRUE(initial.has_value());
    EXPECT_FALSE(sync.Push(0, Surface(0, 200ms, kSurfaceBytes, 2)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 201ms, kSurfaceBytes, 1)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 202ms, kSurfaceBytes, 2)));
    EXPECT_FALSE(sync.Synchronized());
    EXPECT_FALSE(sync.Push(0, Surface(0, 234ms, kSurfaceBytes, 2)));
    auto fresh = sync.Push(1, Surface(1, 235ms, kSurfaceBytes, 2));
    EXPECT_TRUE(fresh.has_value());
}

TEST(FramePair, DiscontinuousInputFlushesPendingPartnerAndMarksNextPair) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    auto interrupted = Surface(1, 101ms);
    interrupted.discontinuity = true;
    EXPECT_FALSE(sync.Push(1, std::move(interrupted)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 134ms)));
    auto pair = sync.Push(1, Surface(1, 135ms));
    ASSERT_TRUE(pair.has_value());
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam0.capture_pts, 134ms);
    EXPECT_TRUE(pair.value_or(sst::pipeline::FramePair{}).discontinuity);
}

TEST(FramePair, ChoosesNearestPartnerRatherThanFirstQueuedPartner) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(1, Surface(1, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 115ms)));
    auto pair = sync.Push(0, Surface(0, 110ms));
    ASSERT_TRUE(pair.has_value());
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam1.capture_pts, 115ms);
}

TEST(FramePair, IsUnavailableBeforeFirstValidPair) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Synchronized());
}

TEST(FramePair, MatchesNearestCapturePtsWithinHalfFramePlusTwoMilliseconds) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 133ms)));
    auto pair = sync.Push(1, Surface(1, 115ms));
    if (!pair.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_EQ(pair.value().cam0.camera_index, 0);
    EXPECT_EQ(pair.value().cam1.camera_index, 1);
    EXPECT_EQ(pair.value().cam0.capture_pts, 100ms);
    EXPECT_EQ(pair.value().cam1.capture_pts, 115ms);
    EXPECT_EQ(pair.value().skew, 15ms);
    EXPECT_FALSE(sync.Push(1, Surface(1, 152ms)));
    pair = sync.Push(0, Surface(0, 170ms));
    if (!pair.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_EQ(pair.value().cam0.camera_index, 0);
    EXPECT_EQ(pair.value().cam1.camera_index, 1);
    EXPECT_EQ(pair.value().cam0.capture_pts, 170ms);
    EXPECT_EQ(pair.value().cam1.capture_pts, 152ms);
    EXPECT_EQ(pair.value().skew, 18ms);
}

TEST(FramePair, RejectsNineteenMillisecondsAndNeverReusesPartner) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 119ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 134ms)));
    EXPECT_EQ(sync.Stats().paired, 0);
}

TEST(FramePair, RejectsDuplicateOutOfOrderAndIncomparableEpochs) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 99ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 101ms, 10, 2)));
    EXPECT_FALSE(sync.Synchronized());
    EXPECT_FALSE(sync.Push(0, Surface(0, 200ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 201ms, 10, 2)));
    EXPECT_EQ(sync.Stats().paired, 0);
}

TEST(FramePair, DropsUnmatchedAfterOneFrameAndBoundsPendingInputs) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 134ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 168ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 202ms)));
    EXPECT_LE(sync.Stats().pending[0], 3);
    EXPECT_GT(sync.Stats().dropped[0], 0);
    EXPECT_FALSE(sync.Push(1, Surface(1, 101ms)));
}

TEST(FramePair, DifferentEpochNeverBecomesPairAfterRepeatedPushes) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms, 10, 1)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 101ms, 10, 2)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 134ms, 10, 1)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 135ms, 10, 2)));
    EXPECT_EQ(sync.Stats().paired, 0);
    EXPECT_FALSE(sync.Synchronized());
}

TEST(FramePair, EpochChangeAfterRestartCannotReuseOldCameraFrame) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 100ms, 10, 2)));
    sync.Restart(1);
    EXPECT_FALSE(sync.Push(1, Surface(1, 200ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    auto pair = sync.Push(0, Surface(0, 201ms));
    if (!pair.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_EQ(pair.value().cam1.capture_pts, 200ms);
}

TEST(FramePair, RestartFlushesOldPartnerAndAllowsFreshPair) {
    FramePairSynchronizer sync(kFps);
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    sync.Restart(1);
    EXPECT_FALSE(sync.Push(1, Surface(1, 101ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 200ms)));
    auto pair = sync.Push(1, Surface(1, 202ms));
    if (!pair.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_EQ(pair.value().cam0.capture_pts, 200ms);
}

TEST(FramePairRing, EnforcesCountAndByteBudgetsWithOldestEviction) {
    FramePairRing ring(2, 2 * kPairBytes);
    FramePairSynchronizer sync(kFps);
    for (int i = 0; i < 3; ++i) {
        auto pts = std::chrono::milliseconds(kInitialMs + i * kIntervalMs);
        sync.Push(0, Surface(0, pts));
        auto pair = sync.Push(1, Surface(1, pts));
        if (!pair.has_value()) {
            FAIL() << "expected a paired frame";
            return;
        }
        ring.Push(std::move(pair.value()));
    }
    EXPECT_EQ(ring.Stats().depth, 2);
    EXPECT_EQ(ring.Stats().bytes, 2 * kPairBytes);
    EXPECT_EQ(ring.Stats().dropped, 1);
    auto due = ring.PopDue(2000ms, 1500ms);
    if (!due.has_value()) {
        FAIL() << "expected a due pair";
        return;
    }
    EXPECT_EQ(due.value().cam0.capture_pts, 134ms);
    EXPECT_FALSE(ring.PopDue(1600ms, 1500ms));
}

TEST(FramePairRing, EvictsForBytesEvenWhenCountHasRoom) {
    FramePairRing ring(kFullPairs, kPairBytes + kSurfaceBytes);
    FramePairSynchronizer sync(kFps);
    sync.Push(0, Surface(0, 100ms));
    auto first = sync.Push(1, Surface(1, 100ms));
    if (!first.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_TRUE(ring.Push(std::move(first.value())));
    sync.Push(0, Surface(0, 134ms));
    auto second = sync.Push(1, Surface(1, 134ms));
    if (!second.has_value()) {
        FAIL() << "expected a paired frame";
        return;
    }
    EXPECT_TRUE(ring.Push(std::move(second.value())));
    EXPECT_EQ(ring.Stats().depth, 1);
    EXPECT_EQ(ring.Stats().bytes, kPairBytes);
    EXPECT_EQ(ring.Stats().dropped, 1);
}

TEST(FramePairRing, SupportsThirtyPairMemoryFallbackWithoutGrowing) {
    FramePairRing ring(kFallbackPairs, kFallbackPairs * kPairBytes);
    FramePairSynchronizer sync(kFps);
    for (int i = 0; i < static_cast<int>(kFullPairs); ++i) {
        auto pts = std::chrono::milliseconds(kInitialMs + i * kIntervalMs);
        sync.Push(0, Surface(0, pts));
        auto pair = sync.Push(1, Surface(1, pts));
        if (!pair.has_value()) {
            FAIL() << "expected a paired frame";
            return;
        }
        ring.Push(std::move(pair.value()));
    }
    EXPECT_EQ(ring.Stats().depth, kFallbackPairs);
    EXPECT_EQ(ring.Stats().bytes, kFallbackPairs * kPairBytes);
    EXPECT_EQ(ring.Stats().dropped, kFullPairs - kFallbackPairs);
}

TEST(FramePairRing, RetainsSurfaceUntilExternalReaderAndFenceComplete) {
    std::promise<void> finished;
    std::weak_ptr<void> weak;
    {
        FramePairRing ring(1, 2 * kPairBytes);
        FramePairSynchronizer sync(kFps);
        auto surface = Surface(0, 100ms);
        surface.producer_fence = finished.get_future().share();
        weak = surface.owner;
        sync.Push(0, std::move(surface));
        auto pair = sync.Push(1, Surface(1, 100ms));
        if (!pair.has_value()) {
            FAIL() << "expected a paired frame";
            return;
        }
        ring.Push(std::move(pair.value()));
        auto due = ring.PopDue(2000ms, 1500ms);
        if (!due.has_value()) {
            FAIL() << "expected a due pair";
            return;
        }
        auto reader = due.value().cam0;
        due.reset();
        EXPECT_FALSE(weak.expired());
        finished.set_value();
        reader.WaitForProducer();
    }
    EXPECT_TRUE(weak.expired());
}
}  // namespace
