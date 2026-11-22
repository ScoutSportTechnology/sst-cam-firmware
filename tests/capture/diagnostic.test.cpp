#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <memory>
#include <regex>
#include <sstream>
#include <vector>

#include "app/capture/services/capture-diagnostic.hpp"

namespace sst::capture::test {
namespace {
constexpr std::uint64_t kFramePeriodNs = 33'333'333;
constexpr float kTestLuma = 0.4F;
class DiagnosticHardware final : public argus::Hardware {
   public:
    auto Modes(int /*camera*/) -> std::vector<std::uint64_t> override { return {kFramePeriodNs}; }
    auto Open(int /*camera*/, const argus::Settings& /*settings*/) -> void override { ++opened; }
    auto Acquire(int camera) -> std::optional<argus::Sample> override {
        const auto frame = ++acquired[camera];
        return argus::Sample{
            .slot = frame % argus::kPoolBuffers,
            .request_id = request_id[camera],
            .timestamp_ns = epoch_ns + static_cast<std::uint64_t>(frame) * kFramePeriodNs,
            .luma = kTestLuma,
            .red = 1.F,
            .blue = 1.F,
            .exposure_ns = argus::kFlickerExposureNs,
            .analog_gain = 1.F,
            .isp_gain = 1.F,
            .wb_red = 1.F,
            .wb_blue = 1.F};
    }
    auto Release(std::pair<int, int> /*slot*/) -> void override { ++released; }
    auto Apply(int camera, const argus::Controls& controls) -> void override {
        ++applied;
        last_request[camera] = controls;
        ++request_id[camera];
    }
    auto RequestId(int camera) -> std::uint32_t override { return request_id[camera]; }
    auto Close(int /*camera*/) -> void override { ++closed; }
    int opened = 0;
    int closed = 0;
    int released = 0;
    int applied = 0;
    std::array<int, 2> acquired{};
    std::array<std::uint32_t, 2> request_id{};
    std::array<argus::Controls, 2> last_request{};
    std::uint64_t epoch_ns =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count());
};
}  // namespace

TEST(CaptureDiagnosticTest, CapturesPairsReportsMetricsAndClosesBothCameras) {
    auto hardware = std::make_shared<DiagnosticHardware>();
    std::ostringstream output;
    EXPECT_EQ(RunCaptureDiagnostic(hardware, std::chrono::milliseconds(30), output), 0);
    EXPECT_EQ(hardware->opened, 2);
    EXPECT_EQ(hardware->closed, 2);
    EXPECT_EQ(hardware->released, hardware->acquired[0] + hardware->acquired[1]);
    EXPECT_GT(hardware->applied, 0);
    const auto metrics = output.str();
    EXPECT_TRUE(std::regex_search(metrics, std::regex(R"(paired=[1-9][0-9]*)")));
    EXPECT_TRUE(std::regex_search(metrics, std::regex(R"(pair_coverage_pct=[1-9][0-9]*)")));
    EXPECT_NE(metrics.find("mem_available_min_kib="), std::string::npos);
    EXPECT_NE(metrics.find("ring_target_ms=1500"), std::string::npos);
    EXPECT_NE(metrics.find("skew_p99_ms="), std::string::npos);
    EXPECT_NE(metrics.find("mem_available_kib="), std::string::npos);
    EXPECT_NE(metrics.find("requested_exposure_ns="), std::string::npos);
    EXPECT_NE(metrics.find("applied_exposure_ns="), std::string::npos);
    EXPECT_NE(metrics.find("capture_frame camera=0"), std::string::npos);
    EXPECT_NE(metrics.find("capture_frame camera=1"), std::string::npos);
    EXPECT_NE(metrics.find("steady_fps0="), std::string::npos);
    EXPECT_NE(metrics.find("startup_dropped0="), std::string::npos);
    EXPECT_NE(metrics.find("steady_mismatch0="), std::string::npos);
    EXPECT_NE(metrics.find("ae_converged_frames="), std::string::npos);
    EXPECT_NE(metrics.find("steady_pair_coverage_pct="), std::string::npos);
    EXPECT_NE(metrics.find("steady_id_gaps0="), std::string::npos);
    EXPECT_NE(metrics.find("requested_client_data="), std::string::npos);
}
}  // namespace sst::capture::test
