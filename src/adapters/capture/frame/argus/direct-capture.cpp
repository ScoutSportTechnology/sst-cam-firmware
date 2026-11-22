#include "adapters/capture/frame/argus/direct-capture.hpp"

#include <dlfcn.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace sst::capture::argus {
namespace {
constexpr auto kFlickerNs = kFlickerExposureNs;
constexpr auto kSurfaceBytes = kNv12Bytes;
constexpr float kTargetLuma = 0.45F;
constexpr float kMinLuma = 0.02F;
constexpr float kMaxGainStep = 1.25F;
constexpr float kMinGainStep = 0.8F;
constexpr float kExposureHysteresis = 0.05F;
constexpr float kWbMin = 0.5F;
constexpr float kWbMax = 2.F;
constexpr int kHardwareAbiVersion = 2;
}  // namespace

SharedController::SharedController(std::uint64_t max_exposure_ns)
    : max_exposure_ns_(max_exposure_ns) {
    if (max_exposure_ns < kFlickerNs || max_exposure_ns % kFlickerNs != 0) {
        throw std::invalid_argument("max exposure must be a positive 60 Hz half-cycle multiple");
    }
}
auto SharedController::SetGainLimits(GainLimits limits) -> void {
    limits_ = limits;
    controls_.analog_gain = std::clamp(controls_.analog_gain, limits.analog_min, limits.analog_max);
    controls_.isp_gain = std::clamp(controls_.isp_gain, limits.isp_min, limits.isp_max);
}
auto SharedController::Update(const std::array<Sample, 2>& samples) -> Controls {
    const float luma = std::clamp((samples[0].luma + samples[1].luma) / 2, kMinLuma, 1.F);
    const float ratio = std::clamp(kTargetLuma / luma, kMinGainStep, kMaxGainStep);
    desired_exposure_ns_ = std::clamp(desired_exposure_ns_ * ratio, static_cast<float>(kFlickerNs),
                                      static_cast<float>(max_exposure_ns_));
    const auto steps = static_cast<std::uint64_t>(
        std::round(desired_exposure_ns_ / static_cast<float>(kFlickerNs)));
    controls_.exposure_ns =
        std::clamp<std::uint64_t>(steps, 1, max_exposure_ns_ / kFlickerNs) * kFlickerNs;
    if (std::abs(ratio - 1.F) > kExposureHysteresis &&
        ((ratio > 1.F && controls_.exposure_ns == max_exposure_ns_) ||
         (ratio < 1.F && controls_.exposure_ns == kFlickerNs))) {
        const float gain =
            std::clamp(controls_.analog_gain * ratio, limits_.analog_min, limits_.analog_max);
        controls_.analog_gain = gain;
        if ((gain == limits_.analog_max && ratio > 1.F) ||
            (gain == limits_.analog_min && ratio < 1.F)) {
            controls_.isp_gain =
                std::clamp(controls_.isp_gain * ratio, limits_.isp_min, limits_.isp_max);
        }
    }
    const auto red = std::max((samples[0].red + samples[1].red) / 2, kMinLuma);
    const auto blue = std::max((samples[0].blue + samples[1].blue) / 2, kMinLuma);
    controls_.wb_red =
        std::clamp(controls_.wb_red *
                       std::clamp(1.F / red, 1.F - kExposureHysteresis, 1.F + kExposureHysteresis),
                   kWbMin, kWbMax);
    controls_.wb_blue =
        std::clamp(controls_.wb_blue *
                       std::clamp(1.F / blue, 1.F - kExposureHysteresis, 1.F + kExposureHysteresis),
                   kWbMin, kWbMax);
    return controls_;
}
DirectCapture::DirectCapture(std::shared_ptr<Hardware> hardware, bool deployment_gate,
                             std::size_t max_pairs, std::size_t max_bytes,
                             std::chrono::nanoseconds extra_skew, std::uint64_t max_exposure_ns)
    : hardware_(std::move(hardware)),
      deployment_gate_(deployment_gate),
      controller_(max_exposure_ns),
      sync_(sst::pipeline::FramePairSynchronizer::kDefaultFps, extra_skew),
      ring_(max_pairs, max_bytes) {
    if (!hardware_) {
        throw std::invalid_argument("capture hardware missing");
    }
    lifetime_ = std::make_shared<SessionLifetime>();
    lifetime_->hardware = hardware_;
}
DirectCapture::~DirectCapture() { Stop(); }
auto DirectCapture::Start() -> void {
    if (!deployment_gate_) {
        throw std::runtime_error("direct Argus deployment gate disabled");
    }
    if (!previous_lifetime_.expired() || lifetime_.use_count() != 1 || lifetime_->opened[0] ||
        lifetime_->opened[1]) {
        throw std::runtime_error("direct Argus session still in use");
    }
    GainLimits shared_limits;
    for (int camera = 0; camera < 2; ++camera) {
        std::vector<std::uint64_t> modes;
        try {
            modes = hardware_->Modes(camera);
        } catch (...) {
            Stop();
            throw;
        }
        auto selected = std::find_if(modes.begin(), modes.end(), [](auto duration_ns) {
            constexpr std::uint64_t kMinDuration = 33'300'000;
            constexpr std::uint64_t kMaxDuration = 33'366'667;
            return duration_ns >= kMinDuration && duration_ns <= kMaxDuration;
        });
        if (selected == modes.end()) {
            Stop();
            throw std::runtime_error("no 4K30 sensor mode within 0.1 percent");
        }
        try {
            hardware_->Open(camera, Settings{.duration_ns = *selected});
            lifetime_->opened[camera] = true;
            const auto limits = hardware_->Limits(camera);
            shared_limits.analog_min = std::max(shared_limits.analog_min, limits.analog_min);
            shared_limits.analog_max = std::min(shared_limits.analog_max, limits.analog_max);
            shared_limits.isp_min = std::max(shared_limits.isp_min, limits.isp_min);
            shared_limits.isp_max = std::min(shared_limits.isp_max, limits.isp_max);
        } catch (...) {
            hardware_->Close(camera);
            Stop();
            throw;
        }
    }
    if (shared_limits.analog_min > shared_limits.analog_max ||
        shared_limits.isp_min > shared_limits.isp_max) {
        Stop();
        throw std::runtime_error("camera gain ranges do not overlap");
    }
    controller_.SetGainLimits(shared_limits);
}
auto DirectCapture::Poll(int camera) -> void {
    if (camera < 0 || camera > 1 || !lifetime_->opened[camera]) {
        throw std::runtime_error("camera not open");
    }
    auto sample = hardware_->Acquire(camera);
    if (!sample) {
        return;
    }
    if (sample->timestamp_ns == 0 || sample->timestamp_ns <= last_timestamp_[camera]) {
        hardware_->Release({camera, sample->slot});
        Reset(camera);
        return;
    }
    const auto other = 1 - camera;
    constexpr std::uint64_t kEpochToleranceNs = 2'000'000'000;
    if (last_timestamp_[other] != 0 &&
        (sample->timestamp_ns > last_timestamp_[other] + kEpochToleranceNs ||
         last_timestamp_[other] > sample->timestamp_ns + kEpochToleranceNs)) {
        hardware_->Release({camera, sample->slot});
        Reset(camera);
        return;
    }
    last_timestamp_[camera] = sample->timestamp_ns;
    if (clock_epoch_[camera] == 0) {
        clock_epoch_[camera] = epoch_;
    }
    samples_[camera] = *sample;
    auto lifetime = lifetime_;
    const auto slot = sample->slot;
    VideoSurface surface{
        .owner = std::shared_ptr<void>(
            reinterpret_cast<void*>(1),
            [lifetime, camera, slot](void*) { lifetime->hardware->Release({camera, slot}); }),
        .producer_fence = {},
        .camera_index = static_cast<std::uint16_t>(camera),
        .capture_pts = std::chrono::nanoseconds(sample->timestamp_ns),
        .raw_pts = std::chrono::nanoseconds(sample->timestamp_ns),
        .clock_epoch = clock_epoch_[camera],
        .geometry = {kSensorWidth, kSensorHeight},
        .bytes = kSurfaceBytes};
    auto pair = sync_.Push(static_cast<std::uint16_t>(camera), std::move(surface));
    if (pair.has_value()) {
        ring_.Push(std::move(pair.value()));
    }
    if (samples_[0].has_value() && samples_[1].has_value()) {
        auto controls =
            controller_.Update({samples_[0].value_or(Sample{}), samples_[1].value_or(Sample{})});
        hardware_->Apply(0, controls);
        hardware_->Apply(1, controls);
        samples_[0].reset();
        samples_[1].reset();
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= last_log_[camera] + std::chrono::seconds(1)) {
        spdlog::info("argus camera={} sensor_ts={} exposure_ns={} analog={} isp={} wb={},{}",
                     camera, sample->timestamp_ns, sample->exposure_ns, sample->analog_gain,
                     sample->isp_gain, sample->wb_red, sample->wb_blue);
        last_log_[camera] = now;
    }
}
auto DirectCapture::Reset(int camera) -> void {
    if (camera < 0 || camera > 1) {
        throw std::invalid_argument("invalid camera index");
    }
    sync_.Restart(0);
    sync_.Restart(1);
    ring_.Clear();
    samples_ = {};
    last_timestamp_ = {};
    clock_epoch_ = {};
    ++epoch_;
}
auto DirectCapture::PopDue(std::chrono::nanoseconds now, std::chrono::nanoseconds delay)
    -> std::optional<sst::pipeline::FramePair> {
    return ring_.PopDue(now, delay);
}
auto MakeArgusHardware() -> std::shared_ptr<Hardware> {
    const auto library_path =
        std::filesystem::read_symlink("/proc/self/exe").parent_path() / "libsst_argus_hardware.so";
    void* library = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        throw std::runtime_error("Argus hardware library unavailable");
    }
    auto* abi = reinterpret_cast<int (*)()>(dlsym(library, "ArgusHardwareAbiVersion"));
    if (abi == nullptr || abi() != kHardwareAbiVersion) {
        dlclose(library);
        throw std::runtime_error("Argus hardware library ABI mismatch");
    }
    auto* create = reinterpret_cast<Hardware* (*)()>(dlsym(library, "CreateArgusHardware"));
    if (create == nullptr) {
        dlclose(library);
        throw std::runtime_error("Argus hardware factory unavailable");
    }
    try {
        return {create(), [library](Hardware* hardware) {
                    delete hardware;
                    dlclose(library);
                }};
    } catch (...) {
        dlclose(library);
        throw;
    }
}
auto DirectCapture::Stop() -> void {
    ring_.Clear();
    sync_.Restart(0);
    sync_.Restart(1);
    samples_ = {};
    last_timestamp_ = {};
    clock_epoch_ = {};
    ++epoch_;
    if (lifetime_->opened[0] || lifetime_->opened[1]) {
        previous_lifetime_ = lifetime_;
    }
    lifetime_ = std::make_shared<SessionLifetime>();
    lifetime_->hardware = hardware_;
}
}  // namespace sst::capture::argus
