#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>

#include "adapters/capture/frame/argus/direct-capture.hpp"

namespace {
using namespace std::chrono_literals;
constexpr std::uint64_t kRoundedFrameNs = 33'333'334;
constexpr std::uint64_t kOffsetNs = 1'000'000;
constexpr std::uint64_t kBadModeNs = 40'000'000;
constexpr float kLowLuma = 0.1F;
constexpr float kHighLuma = 0.3F;
constexpr float kRedHigh = 1.2F;
constexpr float kBlueLow = 0.8F;
constexpr int kConvergenceFrames = 30;
using sst::capture::argus::Controls;
using sst::capture::argus::DirectCapture;
using sst::capture::argus::Hardware;
using sst::capture::argus::Sample;
using sst::capture::argus::Settings;

class FakeHardware : public Hardware {
   public:
    std::vector<Settings> configured;
    std::vector<Controls> applied;
    std::vector<std::uint64_t> modes{kRoundedFrameNs, kBadModeNs};
    int released = 0;
    int closed = 0;
    int fail_camera = -1;
    std::uint64_t tick = 5'000'000'000ULL;
    auto Modes(int /*unused*/) -> std::vector<std::uint64_t> override { return modes; }
    auto Open(int camera, const Settings& settings) -> void override {
        if (camera == fail_camera) {
            fail_camera = -1;
            throw std::runtime_error("partial camera open");
        }
        configured.push_back(settings);
        EXPECT_GE(camera, 0);
        EXPECT_LE(camera, 1);
    }
    auto Acquire(int camera) -> std::optional<Sample> override {
        tick += camera == 0 ? kRoundedFrameNs : kOffsetNs;
        return Sample{.slot = 0, .timestamp_ns = tick, .luma = kLowLuma, .red = 1.F, .blue = 1.F};
    }
    auto Release(std::pair<int, int> /*camera_slot*/) -> void override { ++released; }
    auto Apply(int /*camera*/, const Controls& controls) -> void override {
        applied.push_back(controls);
    }
    auto Close(int /*camera*/) -> void override { ++closed; }
};

TEST(DirectArgus, SelectsRoundedThirtyFpsAndAppliesIdenticalManualSettings) {
    auto hardware = std::make_shared<FakeHardware>();
    DirectCapture capture(hardware, true);
    capture.Start();
    ASSERT_EQ(hardware->configured.size(), 2);
    for (const auto& setting : hardware->configured) {
        EXPECT_EQ(setting.duration_ns, kRoundedFrameNs);
        EXPECT_EQ(setting.buffer_count, sst::capture::argus::kPoolBuffers);
        EXPECT_EQ(setting.width, sst::capture::argus::kSensorWidth);
        EXPECT_EQ(setting.height, sst::capture::argus::kSensorHeight);
        EXPECT_EQ(setting.antibanding_hz, sst::capture::argus::kAntibandingHz);
        EXPECT_FLOAT_EQ(setting.denoise_strength, sst::capture::argus::kDenoiseStrength);
        EXPECT_FLOAT_EQ(setting.edge_strength, sst::capture::argus::kEdgeStrength);
    }
    capture.Poll(0);
    capture.Poll(1);
    ASSERT_EQ(hardware->applied.size(), 2);
    EXPECT_EQ(hardware->applied[0].exposure_ns, hardware->applied[1].exposure_ns);
    EXPECT_FLOAT_EQ(hardware->applied[0].analog_gain, hardware->applied[1].analog_gain);
    capture.Stop();
    EXPECT_EQ(hardware->released, 2);
}

TEST(DirectArgus, ReaderRetainsArgusSessionPastCaptureStop) {
    auto hardware = std::make_shared<FakeHardware>();
    auto capture = std::make_unique<DirectCapture>(hardware, true);
    capture->Start();
    capture->Poll(0);
    capture->Poll(1);
    auto reader = capture->PopDue(std::chrono::nanoseconds(hardware->tick) + 2s, 1s);
    ASSERT_TRUE(reader);
    capture->Stop();
    EXPECT_EQ(hardware->released, 0);
    reader.reset();
    EXPECT_EQ(hardware->released, 2);
}

TEST(DirectArgus, RestartWaitsForRetainedReadersBeforeOpeningNewSessions) {
    auto hardware = std::make_shared<FakeHardware>();
    DirectCapture capture(hardware, true);
    capture.Start();
    capture.Poll(0);
    capture.Poll(1);
    auto reader = capture.PopDue(std::chrono::nanoseconds(hardware->tick) + 2s, 1s);
    ASSERT_TRUE(reader);
    capture.Stop();
    capture.Stop();
    EXPECT_THROW(capture.Start(), std::runtime_error);
    EXPECT_EQ(hardware->configured.size(), 2);
    reader.reset();
    EXPECT_EQ(hardware->closed, 2);
    capture.Start();
    EXPECT_EQ(hardware->configured.size(), 4);
    capture.Stop();
}

TEST(DirectArgus, FailedSecondCameraOpenReleasesItsPartialResources) {
    auto hardware = std::make_shared<FakeHardware>();
    hardware->fail_camera = 1;
    DirectCapture capture(hardware, true);
    EXPECT_THROW(capture.Start(), std::runtime_error);
    EXPECT_EQ(hardware->closed, 2);
    capture.Start();
    capture.Stop();
}

TEST(DirectArgus, GateAndUnsupportedModeRejectBeforeOpeningSensors) {
    auto hardware = std::make_shared<FakeHardware>();
    DirectCapture disabled(hardware, false);
    EXPECT_THROW(disabled.Start(), std::runtime_error);
    hardware->modes = {kBadModeNs};
    DirectCapture enabled(hardware, true);
    EXPECT_THROW(enabled.Start(), std::runtime_error);
    EXPECT_TRUE(hardware->configured.empty());
}

TEST(DirectArgusHardware, CapturesComparableSensorTimestampsWithoutCpuMaps) {
    DirectCapture capture(sst::capture::argus::MakeArgusHardware(), true);
    capture.Start();
    for (int i = 0; i < sst::capture::argus::kPoolBuffers; ++i) {
        capture.Poll(0);
        capture.Poll(1);
    }
    EXPECT_GT(capture.Stats().depth, 0);
    capture.Stop();
}

TEST(SharedArgusControls, BothViewsBoundStepsAndConverge) {
    sst::capture::argus::SharedController controller;
    auto first = controller.Update({Sample{.luma = kLowLuma, .red = kRedHigh, .blue = kBlueLow},
                                    Sample{.luma = kHighLuma, .red = kBlueLow, .blue = kRedHigh}});
    EXPECT_LE(first.exposure_ns, 16'666'667ULL);
    EXPECT_GE(first.exposure_ns, 8'333'333ULL);
    for (int i = 0; i < kConvergenceFrames; ++i) {
        first = controller.Update({Sample{.luma = kLowLuma, .red = kRedHigh, .blue = kBlueLow},
                                   Sample{.luma = kHighLuma, .red = kBlueLow, .blue = kRedHigh}});
    }
    EXPECT_GT(first.analog_gain * first.isp_gain, 1.0F);
    EXPECT_LE(first.exposure_ns, 25'000'000ULL);
    EXPECT_NEAR(first.wb_red, 1.F, .05F);
    EXPECT_NEAR(first.wb_blue, 1.F, .05F);
}
}
