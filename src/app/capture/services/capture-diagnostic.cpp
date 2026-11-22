#include "app/capture/services/capture-diagnostic.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
constexpr auto kWarmup = std::chrono::seconds(5);
constexpr std::uint64_t kExposureToleranceNs = 2;
constexpr float kGainTolerance = 0.02F;
constexpr float kConvergenceTolerance = 0.05F;
constexpr float kTargetLuma = 0.45F;
constexpr double kNanosecondsPerSecond = 1'000'000'000.0;
constexpr std::uint64_t kFramePeriodNs = 33'333'333;
constexpr std::uint64_t kGapThresholdNs = 50'000'000;
constexpr std::size_t kMaxSkewSamples = 100'000;
constexpr double kNanosecondsPerMillisecond = 1'000'000.0;

class MeasuredHardware final : public argus::Hardware {
   public:
    MeasuredHardware(std::shared_ptr<argus::Hardware> delegate, std::ostream& output)
        : delegate_(std::move(delegate)), output_(output) {}
    auto Modes(int camera) -> std::vector<std::uint64_t> override {
        return delegate_->Modes(camera);
    }
    auto Open(int camera, const argus::Settings& settings) -> void override {
        delegate_->Open(camera, settings);
    }
    auto Limits(int camera) -> argus::GainLimits override { return delegate_->Limits(camera); }
    auto Acquire(int camera) -> std::optional<argus::Sample> override {
        auto sample = delegate_->Acquire(camera);
        if (sample) {
            Record(camera, *sample);
        }
        return sample;
    }
    auto Record(int camera, const argus::Sample& value) -> void {
        const auto* sample = &value;
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
        const bool id_gap = sample->capture_id != 0 && last_capture_id_[camera] != 0 &&
                            sample->capture_id != last_capture_id_[camera] + 1;
        id_gaps_[camera] += id_gap ? 1 : 0;
        last_capture_id_[camera] = sample->capture_id;
        ++frames_[camera];
        applied_[camera] = *sample;
        if (first_timestamp_ == 0) {
            first_timestamp_ = sample->timestamp_ns;
        }
        const bool steady =
            sample->timestamp_ns >=
            first_timestamp_ +
                std::chrono::duration_cast<std::chrono::nanoseconds>(kWarmup).count();
        CountWindow(camera, *sample, steady, id_gap);
        const auto& request = requested_[camera];
        const bool mismatch =
            requested_count_[camera] > 0 &&
            ((sample->exposure_ns > request.exposure_ns
                  ? sample->exposure_ns - request.exposure_ns
                  : request.exposure_ns - sample->exposure_ns) > kExposureToleranceNs ||
             std::abs(sample->analog_gain - request.analog_gain) > kGainTolerance ||
             std::abs(sample->isp_gain - request.isp_gain) > kGainTolerance ||
             std::abs(sample->wb_red - request.wb_red) > kGainTolerance ||
             std::abs(sample->wb_blue - request.wb_blue) > kGainTolerance);
        steady_mismatch_[camera] += steady && mismatch ? 1 : 0;
        if (converged_frame_ == 0 &&
            std::abs(sample->luma - kTargetLuma) <= kConvergenceTolerance) {
            converged_frame_ = frames_[camera];
            converged_seconds_ = static_cast<double>(sample->timestamp_ns - first_timestamp_) /
                                 kNanosecondsPerSecond;
        }
        output_ << "capture_frame camera=" << camera << " id=" << sample->capture_id
                << " sensor_ts=" << sample->timestamp_ns << " steady=" << steady
                << " requested_seq=" << requested_count_[camera]
                << " requested_exposure_ns=" << request.exposure_ns
                << " applied_exposure_ns=" << sample->exposure_ns
                << " requested_analog=" << request.analog_gain
                << " applied_analog=" << sample->analog_gain
                << " requested_isp=" << request.isp_gain << " applied_isp=" << sample->isp_gain
                << " requested_wb=" << request.wb_red << ',' << request.wb_blue
                << " applied_wb=" << sample->wb_red << ',' << sample->wb_blue
                << " mismatch=" << mismatch << '\n';
    }
    auto CountWindow(int camera, const argus::Sample& sample, bool steady, bool id_gap) -> void {
        if (!steady) {
            ++warmup_frames_[camera];
            startup_dropped_[camera] = dropped_[camera];
            return;
        }
        if (first_steady_timestamp_ == 0) {
            first_steady_timestamp_ = sample.timestamp_ns;
        }
        ++steady_frames_[camera];
        steady_id_gaps_[camera] += id_gap ? 1 : 0;
        const auto previous = last_steady_timestamp_[camera];
        if (previous != 0 && sample.timestamp_ns <= previous) {
            ++steady_gaps_[camera];
        } else if (previous != 0 && sample.timestamp_ns > previous + kGapThresholdNs) {
            ++steady_gaps_[camera];
            steady_dropped_[camera] +=
                (sample.timestamp_ns - previous + kFramePeriodNs / 2) / kFramePeriodNs - 1;
        }
        last_steady_timestamp_[camera] = sample.timestamp_ns;
    }
    auto Release(std::pair<int, int> slot) -> void override { delegate_->Release(slot); }
    auto Apply(int camera, const argus::Controls& controls) -> void override {
        delegate_->Apply(camera, controls);
        requested_[camera] = controls;
        ++requested_count_[camera];
    }
    auto Close(int camera) -> void override { delegate_->Close(camera); }

    std::array<std::uint64_t, 2> frames_{};
    std::array<std::uint64_t, 2> gaps_{};
    std::array<std::uint64_t, 2> dropped_{};
    std::array<std::uint64_t, 2> last_timestamp_{};
    std::array<argus::Sample, 2> applied_{};
    std::array<argus::Controls, 2> requested_{};
    std::array<std::uint64_t, 2> requested_count_{};
    std::array<std::uint64_t, 2> steady_frames_{};
    std::array<std::uint64_t, 2> warmup_frames_{};
    std::array<std::uint64_t, 2> steady_gaps_{};
    std::array<std::uint64_t, 2> steady_dropped_{};
    std::array<std::uint64_t, 2> startup_dropped_{};
    std::array<std::uint64_t, 2> last_steady_timestamp_{};
    std::array<std::uint64_t, 2> steady_mismatch_{};
    std::array<std::uint64_t, 2> last_capture_id_{};
    std::array<std::uint64_t, 2> id_gaps_{};
    std::array<std::uint64_t, 2> steady_id_gaps_{};
    std::uint64_t first_timestamp_{0};
    std::uint64_t first_steady_timestamp_{0};
    std::uint64_t converged_frame_{0};
    double converged_seconds_{0};

   private:
    std::shared_ptr<argus::Hardware> delegate_;
    std::ostream& output_;
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
    const auto steady_seconds =
        hardware.first_steady_timestamp_ == 0
            ? 0.0
            : std::max(0.0, elapsed_seconds - static_cast<double>(hardware.first_steady_timestamp_ -
                                                                  hardware.first_timestamp_) /
                                                  kNanosecondsPerSecond);
    std::sort(skews.begin(), skews.end());
    const double p99 = skews.empty() ? 0 : skews[(skews.size() - 1) * 99 / 100];
    const auto ring = capture.Stats();
    // Pair coverage uses the busier sensor's expected frame count as denominator.
    const auto expected = std::max(hardware.frames_[0] + hardware.dropped_[0],
                                   hardware.frames_[1] + hardware.dropped_[1]);
    const auto captured = paired + ring.depth;
    const auto steady_expected = std::max(hardware.steady_frames_[0] + hardware.steady_dropped_[0],
                                          hardware.steady_frames_[1] + hardware.steady_dropped_[1]);
    const auto steady_pairs =
        capture.PairingStats().paired >
                std::max(hardware.warmup_frames_[0], hardware.warmup_frames_[1])
            ? capture.PairingStats().paired -
                  std::max(hardware.warmup_frames_[0], hardware.warmup_frames_[1])
            : 0;
    const auto steady_coverage = steady_expected == 0 ? 0.0
                                                      : 100.0 * static_cast<double>(steady_pairs) /
                                                            static_cast<double>(steady_expected);
    const auto coverage =
        expected == 0 ? 0.0 : 100.0 * static_cast<double>(captured) / static_cast<double>(expected);
    // `dropped` is inferred from sensor timestamp gaps; it is not an Argus drop counter.
    output << "capture_diag elapsed_s=" << elapsed_seconds
           << " fps0=" << static_cast<double>(hardware.frames_[0]) / elapsed_seconds
           << " fps1=" << static_cast<double>(hardware.frames_[1]) / elapsed_seconds
           << " steady_fps0="
           << static_cast<double>(hardware.steady_frames_[0]) / std::max(steady_seconds, 1.0)
           << " steady_fps1="
           << static_cast<double>(hardware.steady_frames_[1]) / std::max(steady_seconds, 1.0)
           << " steady_frames0=" << hardware.steady_frames_[0]
           << " steady_frames1=" << hardware.steady_frames_[1]
           << " steady_elapsed_s=" << steady_seconds
           << " startup_dropped0=" << hardware.startup_dropped_[0]
           << " startup_dropped1=" << hardware.startup_dropped_[1]
           << " steady_dropped0=" << hardware.steady_dropped_[0]
           << " steady_dropped1=" << hardware.steady_dropped_[1]
           << " steady_gaps0=" << hardware.steady_gaps_[0]
           << " steady_gaps1=" << hardware.steady_gaps_[1]
           << " capture_id_gaps0=" << hardware.id_gaps_[0]
           << " capture_id_gaps1=" << hardware.id_gaps_[1]
           << " steady_id_gaps0=" << hardware.steady_id_gaps_[0]
           << " steady_id_gaps1=" << hardware.steady_id_gaps_[1]
           << " steady_mismatch0=" << hardware.steady_mismatch_[0]
           << " steady_mismatch1=" << hardware.steady_mismatch_[1]
           << " ae_converged_frames=" << hardware.converged_frame_
           << " ae_converged_seconds=" << hardware.converged_seconds_
           << " dropped0=" << hardware.dropped_[0] << " dropped1=" << hardware.dropped_[1]
           << " gaps0=" << hardware.gaps_[0] << " gaps1=" << hardware.gaps_[1]
           << " paired=" << captured << " pair_coverage_pct=" << coverage
           << " steady_pair_coverage_pct=" << steady_coverage << " ring_dropped=" << ring.dropped
           << " ring_depth=" << ring.depth << " ring_target_ms=1500" << " skew_p99_ms=" << p99
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
    auto measured = std::make_shared<MeasuredHardware>(std::move(hardware), output);
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
