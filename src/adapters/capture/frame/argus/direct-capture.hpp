#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "domain/pipeline/services/frame-pair-ring.hpp"

namespace sst::capture::argus {
inline constexpr int kSensorWidth = 3840;
inline constexpr int kSensorHeight = 2160;
inline constexpr int kPoolBuffers = 60;
inline constexpr int kRingPairs = 45;
inline constexpr int kAntibandingHz = 60;
inline constexpr float kDenoiseStrength = 0.9F;
inline constexpr float kEdgeStrength = 0.4F;
inline constexpr std::uint64_t kFlickerExposureNs = 8'333'333;
inline constexpr std::size_t kNv12Bytes =
    static_cast<std::size_t>(kSensorWidth) * kSensorHeight * 3 / 2;
struct Settings {
    std::uint64_t duration_ns{0};
    int buffer_count{kPoolBuffers};
    int width{kSensorWidth};
    int height{kSensorHeight};
    int antibanding_hz{kAntibandingHz};
    float denoise_strength{kDenoiseStrength};
    float edge_strength{kEdgeStrength};
};
struct Controls {
    std::uint64_t exposure_ns{kFlickerExposureNs};
    float analog_gain{1};
    float isp_gain{1};
    float wb_red{1};
    float wb_blue{1};
};
struct Sample {
    int slot{0};
    std::uint64_t timestamp_ns{0};
    float luma{0};
    float red{1};
    float blue{1};
    std::uint64_t exposure_ns{0};
    float analog_gain{0};
    float isp_gain{0};
    float wb_red{0};
    float wb_blue{0};
};
class Hardware {
   public:
    virtual ~Hardware() = default;
    virtual auto Modes(int camera) -> std::vector<std::uint64_t> = 0;
    virtual auto Open(int camera, const Settings& settings) -> void = 0;
    virtual auto Acquire(int camera) -> std::optional<Sample> = 0;
    virtual auto Release(std::pair<int, int> camera_slot) -> void = 0;
    virtual auto Apply(int camera, const Controls& controls) -> void = 0;
    virtual auto Close(int camera) -> void = 0;
};
class SharedController {
   public:
    auto Update(const std::array<Sample, 2>& samples) -> Controls;
    [[nodiscard]] auto Current() const -> Controls { return controls_; }

   private:
    Controls controls_;
};
class DirectCapture {
   public:
    explicit DirectCapture(std::shared_ptr<Hardware> hardware, bool deployment_gate,
                           std::size_t max_pairs = kRingPairs,
                           std::size_t max_bytes = static_cast<std::size_t>(kRingPairs) * 2 *
                                                   kNv12Bytes);
    ~DirectCapture();
    auto Start() -> void;
    auto Poll(int camera) -> void;
    auto Reset(int camera) -> void;
    auto PopDue(std::chrono::nanoseconds now,
                std::chrono::nanoseconds delay) -> std::optional<sst::pipeline::FramePair>;
    auto Stop() -> void;
    [[nodiscard]] auto Stats() const -> sst::pipeline::FramePairRingStats { return ring_.Stats(); }

   private:
    std::shared_ptr<Hardware> hardware_;
    bool deployment_gate_;
    struct SessionLifetime {
        std::shared_ptr<Hardware> hardware;
        std::array<bool, 2> opened{};
        ~SessionLifetime() {
            for (int i = 0; i < 2; ++i) {
                if (opened[i]) {
                    hardware->Close(i);
                }
            }
        }
    };
    std::shared_ptr<SessionLifetime> lifetime_;
    std::weak_ptr<SessionLifetime> previous_lifetime_;
    std::array<std::optional<Sample>, 2> samples_;
    std::array<std::uint64_t, 2> last_timestamp_{};
    std::array<std::uint64_t, 2> clock_epoch_{};
    std::array<std::chrono::steady_clock::time_point, 2> last_log_{};
    SharedController controller_;
    sst::pipeline::FramePairSynchronizer sync_;
    sst::pipeline::FramePairRing ring_;
};

auto MakeArgusHardware() -> std::shared_ptr<Hardware>;
}
