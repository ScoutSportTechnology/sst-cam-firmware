#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "app/decision/services/static_decision/static-decision.hpp"
#include "domain/capture/models/frame.hpp"
#include "domain/decision/models/camera-choice.hpp"
#include "domain/processing/models/frame-bundle.hpp"

namespace {

using sst::capture::Frame;
using sst::decision::CameraChoice;
using sst::decision::StaticDecision;
using sst::processing::FrameBundle;

// Build a bundle whose source frame carries the given geometry. StaticDecision
// only reads source geometry, so the planes/owner are intentionally empty.
auto BundleWithGeometry(std::uint32_t width, std::uint32_t height) -> FrameBundle {
    FrameBundle bundle;
    bundle.source_frame.geometry = {.width = width, .height = height};
    return bundle;
}

TEST(StaticDecisionTest, PicksCamera0FullFrameForVariousSizes) {
    StaticDecision decision;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes = {
        {1920, 1080}, {1280, 720}, {640, 480}, {3840, 2160}, {1, 1}};

    for (const auto& [width, height] : sizes) {
        std::vector<std::optional<FrameBundle>> cameras = {BundleWithGeometry(width, height),
                                                           BundleWithGeometry(800, 600)};
        const auto choice = decision.Decide(cameras);
        ASSERT_TRUE(choice.has_value()) << "expected a choice for " << width << "x" << height;
        EXPECT_EQ(choice->camera_index, 0U);
        EXPECT_EQ(choice->crop.x, 0U);
        EXPECT_EQ(choice->crop.y, 0U);
        EXPECT_EQ(choice->crop.width, width);
        EXPECT_EQ(choice->crop.height, height);
    }
}

TEST(StaticDecisionTest, IgnoresCamera1WhenCamera0Present) {
    StaticDecision decision;
    std::vector<std::optional<FrameBundle>> cameras = {BundleWithGeometry(1920, 1080),
                                                       BundleWithGeometry(1280, 720)};
    const auto choice = decision.Decide(cameras);
    ASSERT_TRUE(choice.has_value());
    // Always cam 0 even though cam 1 also has a (differently-sized) frame.
    EXPECT_EQ(choice->camera_index, 0U);
    EXPECT_EQ(choice->crop.width, 1920U);
    EXPECT_EQ(choice->crop.height, 1080U);
}

TEST(StaticDecisionTest, ReturnsNulloptWhenCamera0Absent) {
    StaticDecision decision;
    // Camera 0 has no frame this tick; camera 1 does. Static policy serves only
    // camera 0, so there is nothing to present.
    std::vector<std::optional<FrameBundle>> cameras = {std::nullopt, BundleWithGeometry(1280, 720)};
    EXPECT_FALSE(decision.Decide(cameras).has_value());
}

TEST(StaticDecisionTest, ReturnsNulloptWhenNoCameras) {
    StaticDecision decision;
    const std::vector<std::optional<FrameBundle>> empty;
    EXPECT_FALSE(decision.Decide(empty).has_value());
}

TEST(StaticDecisionTest, DegenerateGeometryIsDefinedNotCrash) {
    StaticDecision decision;
    // Zero width or height → defined behavior: skip the tick, no crash.
    std::vector<std::optional<FrameBundle>> zero_width = {BundleWithGeometry(0, 1080)};
    std::vector<std::optional<FrameBundle>> zero_height = {BundleWithGeometry(1920, 0)};
    std::vector<std::optional<FrameBundle>> zero_both = {BundleWithGeometry(0, 0)};
    EXPECT_FALSE(decision.Decide(zero_width).has_value());
    EXPECT_FALSE(decision.Decide(zero_height).has_value());
    EXPECT_FALSE(decision.Decide(zero_both).has_value());
}

}  // namespace
