#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "adapters/capture/frame/argus/direct-capture.hpp"
#include "domain/capture/models/video-surface.hpp"
#include "domain/pipeline/services/frame-pair-ring.hpp"
#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

namespace {
using namespace std::chrono_literals;
constexpr std::size_t kSurfaceBytes = sst::capture::argus::kNv12Bytes;
constexpr std::size_t kFallbackPairs = 30;
constexpr std::size_t kFullPairs = 45;
using sst::capture::VideoSurface;
using sst::pipeline::FramePairRing;
using sst::pipeline::FramePairSynchronizer;

auto Surface(int camera, std::chrono::nanoseconds pts, std::uint64_t epoch = 1) -> VideoSurface {
    return {.owner = std::make_shared<int>(1),
            .producer_fence = {},
            .camera_index = static_cast<std::uint16_t>(camera),
            .capture_pts = pts,
            .raw_pts = pts,
            .clock_epoch = epoch,
            .geometry = {sst::capture::argus::kSensorWidth, sst::capture::argus::kSensorHeight},
            .bytes = kSurfaceBytes};
}

TEST(FramePair, RetainsCounterpartWhenCurrentFrameAlreadyMatched) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 133ms)));
    auto first = sync.Push(1, Surface(1, 101ms));
    ASSERT_TRUE(first);
    EXPECT_EQ(first.value_or(sst::pipeline::FramePair{}).cam0.capture_pts, 100ms);
    auto second = sync.Push(1, Surface(1, 134ms));
    ASSERT_TRUE(second);
    EXPECT_EQ(second.value_or(sst::pipeline::FramePair{}).cam0.capture_pts, 133ms);
    EXPECT_EQ(sync.Stats().paired, 2);
}

TEST(FramePair, NearestWithinThresholdOnly) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(1, Surface(1, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 115ms)));
    EXPECT_EQ(sync.Stats().pending[1], 2);
    auto pair = sync.Push(0, Surface(0, 110ms));
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam1.capture_pts, 115ms);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).skew, 5ms);
    EXPECT_FALSE(sync.Push(0, Surface(0, 200ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 219ms)));
    EXPECT_EQ(sync.Stats().paired, 1);
}

TEST(FramePair, AcceptsEighteenMillisecondsButRejectsNineteen) {
    FramePairSynchronizer accepted;
    EXPECT_FALSE(accepted.Push(0, Surface(0, 100ms)));
    auto pair = accepted.Push(1, Surface(1, 118ms));
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).skew, 18ms);

    FramePairSynchronizer rejected;
    EXPECT_FALSE(rejected.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(rejected.Push(1, Surface(1, 119ms)));
    EXPECT_EQ(rejected.Stats().paired, 0);
}

TEST(FramePair, RejectsOldEpochDuplicateAndRestartedPartner) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 101ms, 2)));
    EXPECT_FALSE(sync.Synchronized());
    sync.Restart(1);
    EXPECT_FALSE(sync.Push(1, Surface(1, 200ms)));
    auto pair = sync.Push(0, Surface(0, 201ms));
    ASSERT_TRUE(pair);
    EXPECT_TRUE(pair.value_or(sst::pipeline::FramePair{}).discontinuity);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam0.capture_pts, 201ms);
}

TEST(FramePair, KeepsFuturePartnerAfterRejectingOlderUnmatchedFrame) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(1, Surface(1, 100ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 132ms)));
    auto pair = sync.Push(0, Surface(0, 129ms));
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam1.capture_pts, 132ms);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).skew, 3ms);
}

TEST(FramePair, RetainsCounterpartAcrossOneFrameArrivalLag) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 134ms)));
    auto pair = sync.Push(1, Surface(1, 101ms));
    ASSERT_TRUE(pair);
    EXPECT_EQ(pair.value_or(sst::pipeline::FramePair{}).cam0.capture_pts, 100ms);
    EXPECT_EQ(sync.Stats().paired, 1);
}

TEST(FramePair, DropsUnmatchedWhenCounterpartIsNewer) {
    FramePairSynchronizer sync;
    EXPECT_FALSE(sync.Push(0, Surface(0, 100ms)));
    EXPECT_FALSE(sync.Push(0, Surface(0, 134ms)));
    EXPECT_FALSE(sync.Push(1, Surface(1, 167ms)));
    EXPECT_GT(sync.Stats().dropped[0], 0);
}

TEST(FramePairRing, BoundedByPairsAndBytesAndRetainsReaders) {
    FramePairSynchronizer sync;
    FramePairRing ring(2, 4 * kSurfaceBytes);
    std::weak_ptr<void> oldest;
    for (int i = 0; i < 3; ++i) {
        auto pts = 100ms + i * 34ms;
        auto surface = Surface(0, pts);
        if (i == 0) {
            oldest = surface.owner;
        }
        sync.Push(0, std::move(surface));
        auto pair = sync.Push(1, Surface(1, pts));
        ASSERT_TRUE(pair);
        EXPECT_TRUE(ring.Push(pair.value_or(sst::pipeline::FramePair{})));
    }
    EXPECT_TRUE(oldest.expired());
    EXPECT_EQ(ring.Stats().depth, 2);
    EXPECT_EQ(ring.Stats().dropped, 1);
    auto due = ring.PopDue(1700ms, 1500ms);
    ASSERT_TRUE(due);
    EXPECT_EQ(due.value_or(sst::pipeline::FramePair{}).pts, 134ms);
}

TEST(FramePairRing, EvictionDefersReturnUntilReaderFinishes) {
    FramePairRing ring(1, 4 * kSurfaceBytes);
    int released = 0;
    auto make = [&](int index, int timestamp) {
        auto left = Surface(0, std::chrono::milliseconds(timestamp));
        left.owner = std::shared_ptr<void>(new int(index), [&](void* ptr) {
            ++released;
            delete static_cast<int*>(ptr);
        });
        auto right = Surface(1, std::chrono::milliseconds(timestamp));
        return sst::pipeline::FramePair{.cam0 = std::move(left),
                                        .cam1 = std::move(right),
                                        .pts = std::chrono::milliseconds(timestamp)};
    };
    auto first = make(1, static_cast<int>((100ms).count()));
    auto reader = first.cam0.owner;
    EXPECT_TRUE(ring.Push(std::move(first)));
    EXPECT_TRUE(ring.Push(make(2, static_cast<int>((134ms).count()))));
    EXPECT_EQ(released, 0);
    reader.reset();
    EXPECT_EQ(released, 1);
}

TEST(FramePairRing, ThirtyPairFallbackIsBounded) {
    FramePairSynchronizer sync;
    FramePairRing ring(kFallbackPairs, 2 * kFallbackPairs * kSurfaceBytes);
    for (int i = 0; i < static_cast<int>(kFullPairs); ++i) {
        auto pts = 100ms + i * 34ms;
        sync.Push(0, Surface(0, pts));
        auto pair = sync.Push(1, Surface(1, pts));
        ASSERT_TRUE(pair);
        ASSERT_TRUE(ring.Push(pair.value_or(sst::pipeline::FramePair{})));
    }
    EXPECT_EQ(ring.Stats().depth, kFallbackPairs);
    EXPECT_EQ(ring.Stats().dropped, kFullPairs - kFallbackPairs);
}
}  // namespace
