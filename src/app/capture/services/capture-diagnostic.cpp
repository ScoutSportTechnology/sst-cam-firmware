#include "app/capture/services/capture-diagnostic.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sst::capture {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto kReportInterval = std::chrono::seconds(10);
constexpr auto kRingDelay = std::chrono::milliseconds(1500);
constexpr std::uint64_t kFramePeriodNs = 33'333'333;
constexpr std::uint64_t kGapThresholdNs = 50'000'000;
constexpr std::size_t kMaxSkewSamples = 100'000;
constexpr double kNanosecondsPerMillisecond = 1'000'000.0;

class MeasuredHardware final : public argus::Hardware {
   public:
    explicit MeasuredHardware(std::shared_ptr<argus::Hardware> delegate)
        : delegate_(std::move(delegate)) {}
    auto Modes(int camera) -> std::vector<std::uint64_t> override {
        return delegate_->Modes(camera);
    }
    auto Open(int camera, const argus::Settings& settings) -> void override {
        delegate_->Open(camera, settings);
    }
    auto Acquire(int camera) -> std::optional<argus::Sample> override {
        auto sample = delegate_->Acquire(camera);
        if (sample) {
            auto& previous = last_timestamp_[camera];
            if (previous != 0) {
                if (sample->timestamp_ns <= previous) {
                    ++gaps_[camera];
                } else if (sample->timestamp_ns > previous + kGapThresholdNs) {
                    ++gaps_[camera];
                    dropped_[camera] +=
                        (sample->timestamp_ns - previous + kFramePeriodNs / 2) / kFramePeriodNs - 1;
                }
            }
            previous = sample->timestamp_ns;
            ++frames_[camera];
            applied_[camera] = *sample;
        }
        return sample;
    }
    auto Release(std::pair<int, int> slot) -> void override { delegate_->Release(slot); }
    auto Apply(int camera, const argus::Controls& controls) -> void override {
        delegate_->Apply(camera, controls);
        requested_[camera] = controls;
    }
    auto Close(int camera) -> void override { delegate_->Close(camera); }

    std::array<std::uint64_t, 2> frames_{};
    std::array<std::uint64_t, 2> gaps_{};
    std::array<std::uint64_t, 2> dropped_{};
    std::array<std::uint64_t, 2> last_timestamp_{};
    std::array<argus::Sample, 2> applied_{};
    std::array<argus::Controls, 2> requested_{};

   private:
    std::shared_ptr<argus::Hardware> delegate_;
};

auto MemAvailableKib() -> std::uint64_t {
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    std::uint64_t value = 0;
    while (meminfo >> key >> value) {
        if (key == "MemAvailable:") {
            return value;
        }
        std::string unit;
        std::getline(meminfo, unit);
    }
    return 0;
}

auto Report(std::ostream& output, const MeasuredHardware& hardware,
            const argus::DirectCapture& capture, std::vector<double>& skews, std::uint64_t paired,
            std::chrono::duration<double> elapsed, std::uint64_t min_mem_kib) -> void {
    const auto elapsed_seconds = elapsed.count();
    std::sort(skews.begin(), skews.end());
    const double p99 = skews.empty() ? 0 : skews[(skews.size() - 1) * 99 / 100];
    const auto ring = capture.Stats();
    // Pair coverage uses the busier sensor's expected frame count as denominator.
    const auto expected = std::max(hardware.frames_[0] + hardware.dropped_[0],
                                   hardware.frames_[1] + hardware.dropped_[1]);
    const auto captured = paired + ring.depth;
    const auto coverage =
        expected == 0 ? 0.0 : 100.0 * static_cast<double>(captured) / static_cast<double>(expected);
    // `dropped` is inferred from sensor timestamp gaps; it is not an Argus drop counter.
    output << "capture_diag elapsed_s=" << elapsed_seconds
           << " fps0=" << static_cast<double>(hardware.frames_[0]) / elapsed_seconds
           << " fps1=" << static_cast<double>(hardware.frames_[1]) / elapsed_seconds
           << " dropped0=" << hardware.dropped_[0] << " dropped1=" << hardware.dropped_[1]
           << " gaps0=" << hardware.gaps_[0] << " gaps1=" << hardware.gaps_[1]
           << " paired=" << captured << " pair_coverage_pct=" << coverage
           << " ring_dropped=" << ring.dropped << " ring_depth=" << ring.depth
           << " ring_target_ms=1500" << " skew_p99_ms=" << p99
           << " mem_available_kib=" << MemAvailableKib() << " mem_available_min_kib=" << min_mem_kib
           << " requested_exposure_ns=" << hardware.requested_[0].exposure_ns << ','
           << hardware.requested_[1].exposure_ns
           << " applied_exposure_ns=" << hardware.applied_[0].exposure_ns << ','
           << hardware.applied_[1].exposure_ns
           << " requested_analog=" << hardware.requested_[0].analog_gain << ','
           << hardware.requested_[1].analog_gain
           << " applied_analog=" << hardware.applied_[0].analog_gain << ','
           << hardware.applied_[1].analog_gain
           << " requested_isp=" << hardware.requested_[0].isp_gain << ','
           << hardware.requested_[1].isp_gain << " applied_isp=" << hardware.applied_[0].isp_gain
           << ',' << hardware.applied_[1].isp_gain
           << " requested_wb_red=" << hardware.requested_[0].wb_red << ','
           << hardware.requested_[1].wb_red << " applied_wb_red=" << hardware.applied_[0].wb_red
           << ',' << hardware.applied_[1].wb_red
           << " requested_wb_blue=" << hardware.requested_[0].wb_blue << ','
           << hardware.requested_[1].wb_blue << " applied_wb_blue=" << hardware.applied_[0].wb_blue
           << ',' << hardware.applied_[1].wb_blue << '\n';
    output.flush();
}
}  // namespace

auto RunCaptureDiagnostic(std::shared_ptr<argus::Hardware> hardware,
                          std::chrono::steady_clock::duration duration,
                          std::ostream& output) -> int {
    if (!hardware || duration <= Clock::duration::zero()) {
        throw std::invalid_argument("capture diagnostic requires hardware and positive duration");
    }
    auto measured = std::make_shared<MeasuredHardware>(std::move(hardware));
    argus::DirectCapture capture(measured, true);
    capture.Start();
    const auto start = Clock::now();
    auto next_report = start + kReportInterval;
    std::uint64_t paired = 0;
    std::uint64_t min_mem_kib = MemAvailableKib();
    std::vector<double> skews;
    auto next_memory_sample = start;
    // Serial hardware acquires may wait up to 200ms each; HIL determines sustainable fps.
    while (Clock::now() - start < duration) {
        // Drain before capture too: a 45-pair ring at 30 fps is exactly 1.5s deep.
        const auto before =
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch());
        while (auto pair = capture.PopDue(before, kRingDelay)) {
            ++paired;
            if (skews.size() < kMaxSkewSamples) {
                skews.push_back(static_cast<double>(pair->skew.count()) /
                                kNanosecondsPerMillisecond);
            }
        }
        capture.Poll(0);
        capture.Poll(1);
        const auto now = Clock::now();
        const auto now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
        while (auto pair = capture.PopDue(now_ns, kRingDelay)) {
            ++paired;
            if (skews.size() < kMaxSkewSamples) {
                skews.push_back(static_cast<double>(pair->skew.count()) /
                                kNanosecondsPerMillisecond);
            }
        }
        if (now >= next_memory_sample) {
            min_mem_kib = std::min(min_mem_kib, MemAvailableKib());
            next_memory_sample = now + std::chrono::seconds(1);
        }
        if (now >= next_report) {
            Report(output, *measured, capture, skews, paired,
                   std::chrono::duration<double>(now - start), min_mem_kib);
            next_report = now + kReportInterval;
        }
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start);
    const auto final_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()) +
        kRingDelay;
    while (auto pair = capture.PopDue(final_ns, kRingDelay)) {
        ++paired;
        if (skews.size() < kMaxSkewSamples) {
            skews.push_back(static_cast<double>(pair->skew.count()) / kNanosecondsPerMillisecond);
        }
    }
    min_mem_kib = std::min(min_mem_kib, MemAvailableKib());
    Report(output, *measured, capture, skews, paired, elapsed, min_mem_kib);
    const bool has_pairs = paired > 0;
    capture.Stop();
    return has_pairs ? 0 : 1;
}
}  // namespace sst::capture
