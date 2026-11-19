#include <Argus/Argus.h>
#include <Argus/Ext/InternalFrameCount.h>
#include <cuda.h>
#include <cudaEGL.h>
#include <cuda_runtime_api.h>
#include <nvbufsurface.h>
#include <png.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Argus;
using Clock = std::chrono::steady_clock;

constexpr uint32_t kWidth = 3840;
constexpr uint32_t kHeight = 2160;
constexpr uint64_t kFrameNs = 1000000000ULL / 30;
constexpr uint64_t kPairThresholdNs = 18666667ULL;
constexpr uint64_t kMinAvailableKiB = 1536ULL * 1024;
constexpr size_t kBuffers = 60;
constexpr size_t kHeldPairs = 45;
constexpr size_t kMaxQueued = 4;

void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}

void cudaCheck(cudaError_t status, const char* message)
{
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(message) + ": " + cudaGetErrorString(status));
}

bool selectFrameDuration(const Range<uint64_t>& range, uint64_t* duration)
{
    if (range.min() == 0 || range.min() > range.max() || range.min() > 33366667ULL)
        return false;
    *duration = std::max(range.min(), std::min(kFrameNs, range.max()));
    return true;
}

size_t windowFor(Clock::duration elapsed)
{
    return elapsed < std::chrono::seconds(5) ? 0 : 1;
}

bool validationPassed(uint64_t passed, uint64_t failed, uint64_t aged, uint64_t snapshots)
{
    return passed > 0 && failed == 0 && aged > 0 && snapshots == 2;
}

struct Slot {
    NvBufSurface* surface = nullptr;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    UniqueObj<Buffer> buffer;

    Slot() = default;
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
    ~Slot()
    {
        buffer.reset();
        if (surface) {
            if (image != EGL_NO_IMAGE_KHR)
                NvBufSurfaceUnMapEglImage(surface, 0);
            NvBufSurfaceDestroy(surface);
        }
    }
};

struct Camera {
    UniqueObj<CaptureSession> session;
    UniqueObj<OutputStream> stream;
    UniqueObj<Request> request;
    std::vector<std::unique_ptr<Slot>> slots;
    IBufferOutputStream* output = nullptr;
    ICaptureSession* capture = nullptr;
};

struct Frame {
    Buffer* buffer;
    uint64_t timestamp;
    uint32_t captureId;
};

struct Pair {
    std::array<Frame, 2> frames;
    std::atomic<int> pendingSamples{0};
    std::array<uint64_t, 2> firstChecksum{};
    bool sampled = false;
    bool selected = false;
    bool recheckQueued = false;
};

struct MetadataSample {
    uint32_t captureId;
    uint64_t timestamp;
    double analogGain;
    double ispDigitalGain;
    double exposureMs;
    double frameMs;
    double denoiseStrength;
    DenoiseMode denoiseMode;
};

const char* denoiseName(const DenoiseMode& mode)
{
    if (mode == DENOISE_MODE_HIGH_QUALITY) return "HIGH_QUALITY";
    if (mode == DENOISE_MODE_FAST) return "FAST";
    if (mode == DENOISE_MODE_OFF) return "OFF";
    return "UNKNOWN";
}

struct Shared {
    std::mutex mutex;
    std::condition_variable ready;
    std::array<std::deque<Frame>, 2> queues;
    std::array<std::vector<MetadataSample>, 2> metadataSamples;
    std::array<uint64_t, 2> received{};
    std::array<std::array<uint64_t, 2>, 2> windowReceived{};
    std::array<std::array<uint64_t, 2>, 2> windowTimestampGaps{};
    std::array<std::array<uint64_t, 2>, 2> windowAcquireErrors{};
    std::array<std::array<uint64_t, 2>, 2> windowQueueDrops{};
    std::array<std::array<uint64_t, 2>, 2> windowCaptureIdGaps{};
    std::array<std::array<uint64_t, 2>, 2> windowInternalGaps{};
    std::array<std::array<uint64_t, 2>, 2> windowReordered{};
    Clock::time_point started;
    std::array<uint64_t, 2> timestampGaps{};
    std::array<uint64_t, 2> queueDrops{};
    std::array<uint64_t, 2> lastTimestamp{};
    std::array<uint64_t, 2> reorderedTimestamps{};
    std::array<uint32_t, 2> lastCaptureId{};
    std::array<bool, 2> captureIdBaseline{};
    std::array<uint64_t, 2> captureIdGaps{};
    std::array<uint64_t, 2> lastInternalCount{};
    std::array<uint64_t, 2> internalGaps{};
    std::array<bool, 2> internalBaseline{};
    std::array<uint64_t, 2> acquisitionTimeouts{};
    std::array<bool, 2> exited{};
    std::array<Clock::time_point, 2> nextMetadataLog{};
    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};
};

void makeCamera(Camera& cam, ICameraProvider* provider, CameraDevice* device, uint32_t index, EGLDisplay display)
{
    auto* properties = interface_cast<ICameraProperties>(device);
    std::vector<SensorMode*> modes;
    check(properties && properties->getAllSensorModes(&modes) == STATUS_OK, "sensor modes unavailable");
    SensorMode* selected = nullptr;
    uint64_t frameDuration = 0;
    for (size_t modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
        auto* info = interface_cast<ISensorMode>(modes[modeIndex]);
        if (!info) continue;
        const auto resolution = info->getResolution();
        const auto range = info->getFrameDurationRange();
        std::printf("sensor=%u mode=%zu resolution=%ux%u frame_duration_ns=[%llu,%llu] max_fps=%.6f\n",
                    index, modeIndex, resolution.width(), resolution.height(),
                    static_cast<unsigned long long>(range.min()),
                    static_cast<unsigned long long>(range.max()),
                    range.min() ? 1000000000.0 / range.min() : 0.0);
        if (!selected && resolution.width() == kWidth && resolution.height() == kHeight &&
            selectFrameDuration(range, &frameDuration))
            selected = modes[modeIndex];
    }
    check(selected != nullptr, "no 3840x2160 sensor mode supporting >=29.5fps");
    cam.session.reset(provider->createCaptureSession(device));
    cam.capture = interface_cast<ICaptureSession>(cam.session);
    check(cam.capture != nullptr, "independent capture session unavailable");

    UniqueObj<OutputStreamSettings> settings(cam.capture->createOutputStreamSettings(STREAM_TYPE_BUFFER));
    auto* bufferSettings = interface_cast<IBufferOutputStreamSettings>(settings);
    check(bufferSettings && bufferSettings->setBufferType(BUFFER_TYPE_EGL_IMAGE) == STATUS_OK,
          "EGLImage buffer stream unavailable");
    bufferSettings->setMetadataEnable(true);
    cam.stream.reset(cam.capture->createOutputStream(settings.get()));
    cam.output = interface_cast<IBufferOutputStream>(cam.stream);
    check(cam.output != nullptr, "buffer output stream unavailable");
    UniqueObj<BufferSettings> imageSettings(cam.output->createBufferSettings());
    auto* eglSettings = interface_cast<IEGLImageBufferSettings>(imageSettings);
    check(eglSettings && eglSettings->setEGLDisplay(display) == STATUS_OK,
          "EGLImage settings unavailable");

    cam.slots.reserve(kBuffers);
    for (size_t n = 0; n < kBuffers; ++n) {
        auto slot = std::make_unique<Slot>();
        NvBufSurfaceAllocateParams params{};
        params.params.width = kWidth;
        params.params.height = kHeight;
        params.params.layout = NVBUF_LAYOUT_PITCH;
        params.params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
        params.params.memType = NVBUF_MEM_SURFACE_ARRAY;
        params.memtag = NvBufSurfaceTag_CAMERA;
        check(NvBufSurfaceAllocate(&slot->surface, 1, &params) == 0 && slot->surface,
              "NV12 NvBufSurface allocation failed");
        check(slot->surface->surfaceList[0].colorFormat == NVBUF_COLOR_FORMAT_NV12,
              "allocated surface is not NV12");
        check(NvBufSurfaceMapEglImage(slot->surface, 0) == 0, "EGLImage mapping failed");
        slot->image = static_cast<EGLImageKHR>(slot->surface->surfaceList[0].mappedAddr.eglImage);
        check(slot->image != EGL_NO_IMAGE_KHR, "EGLImage missing");
        check(eglSettings->setEGLImage(slot->image) == STATUS_OK, "EGLImage setting failed");
        slot->buffer.reset(cam.output->createBuffer(imageSettings.get()));
        check(interface_cast<IEGLImageBuffer>(slot->buffer) != nullptr, "Argus buffer creation failed");
        interface_cast<IBuffer>(slot->buffer)->setClientData(slot.get());
        check(cam.output->releaseBuffer(slot->buffer.get()) == STATUS_OK, "initial buffer release failed");
        cam.slots.push_back(std::move(slot));
    }

    cam.request.reset(cam.capture->createRequest(CAPTURE_INTENT_VIDEO_RECORD));
    auto* req = interface_cast<IRequest>(cam.request);
    auto* source = interface_cast<ISourceSettings>(cam.request);
    auto* autoControl = req ? interface_cast<IAutoControlSettings>(req->getAutoControlSettings()) : nullptr;
    auto* denoise = interface_cast<IDenoiseSettings>(cam.request);
    auto* edge = interface_cast<IEdgeEnhanceSettings>(cam.request);
    check(req && source && autoControl && denoise && edge, "request settings unavailable");
    check(req->enableOutputStream(cam.stream.get()) == STATUS_OK &&
          source->setSensorMode(selected) == STATUS_OK &&
          source->setFrameDurationRange(Range<uint64_t>(frameDuration)) == STATUS_OK &&
          autoControl->setAeAntibandingMode(AE_ANTIBANDING_MODE_60HZ) == STATUS_OK &&
          denoise->setDenoiseMode(DENOISE_MODE_HIGH_QUALITY) == STATUS_OK &&
          denoise->setDenoiseStrength(0.9f) == STATUS_OK &&
          edge->setEdgeEnhanceMode(EDGE_ENHANCE_MODE_FAST) == STATUS_OK &&
          edge->setEdgeEnhanceStrength(0.4f) == STATUS_OK,
          "capture configuration rejected");
    std::printf("sensor=%u selected_mode=3840x2160 requested_duration_ns=%llu requested_fps=%.6f buffers=%zu format=NV12 EGLImage\n",
                index, static_cast<unsigned long long>(frameDuration),
                1000000000.0 / frameDuration, kBuffers);
}

void consume(Camera& cam, Shared& shared, size_t index, bool internalCountAvailable)
{
    while (!shared.stop.load()) {
        Status status = STATUS_OK;
        Buffer* buffer = cam.output->acquireBuffer(200000000ULL, &status);
        if (!buffer) {
            if (!shared.stop.load()) {
                std::lock_guard<std::mutex> guard(shared.mutex);
                ++shared.windowAcquireErrors[windowFor(Clock::now() - shared.started)][index];
            }
            if (status == STATUS_TIMEOUT) {
                std::lock_guard<std::mutex> guard(shared.mutex);
                ++shared.acquisitionTimeouts[index];
                continue;
            }
            if (status != STATUS_END_OF_STREAM && !shared.stop.load()) {
                std::fprintf(stderr, "sensor=%zu acquire failed status=%d\n", index, status);
                shared.failed = true;
            }
            break;
        }
        auto* wrapped = interface_cast<IBuffer>(buffer);
        auto* metadata = wrapped ? interface_cast<const ICaptureMetadata>(wrapped->getMetadata()) : nullptr;
        if (!metadata || metadata->getSensorTimestamp() == 0) {
            std::fprintf(stderr, "sensor=%zu missing sensor timestamp\n", index);
            shared.failed = true;
            cam.output->releaseBuffer(buffer);
            break;
        }
        const Frame frame{buffer, metadata->getSensorTimestamp(), metadata->getCaptureId()};
        const auto* denoise = interface_cast<const IDenoiseMetadata>(wrapped->getMetadata());
        if (!denoise) {
            std::fprintf(stderr, "sensor=%zu applied denoise metadata unavailable\n", index);
            shared.failed = true;
            cam.output->releaseBuffer(buffer);
            break;
        }
        const MetadataSample sample{frame.captureId, frame.timestamp,
                                    metadata->getSensorAnalogGain(), metadata->getIspDigitalGain(),
                                    metadata->getSensorExposureTime() / 1000000.0,
                                    metadata->getFrameDuration() / 1000000.0,
                                    denoise->getDenoiseStrength(), denoise->getDenoiseMode()};
        const auto* internal = internalCountAvailable ?
            interface_cast<const Ext::IInternalFrameCount>(wrapped->getMetadata()) : nullptr;
        const uint64_t internalCount = internal ? internal->getInternalFrameCount() : 0;
        bool overflow = false;
        {
            std::lock_guard<std::mutex> guard(shared.mutex);
            ++shared.received[index];
            const auto now = Clock::now();
            const auto window = windowFor(now - shared.started);
            ++shared.windowReceived[window][index];
            if (now >= shared.nextMetadataLog[index]) {
                shared.metadataSamples[index].push_back(sample);
                shared.nextMetadataLog[index] = now + std::chrono::seconds(1);
            }
            if (shared.lastTimestamp[index] && frame.timestamp <= shared.lastTimestamp[index]) {
                ++shared.reorderedTimestamps[index];
                ++shared.windowReordered[window][index];
            }
            if (shared.lastTimestamp[index] && frame.timestamp > shared.lastTimestamp[index] + kFrameNs * 3 / 2) {
                const auto gaps = (frame.timestamp - shared.lastTimestamp[index] + kFrameNs / 2) / kFrameNs - 1;
                shared.timestampGaps[index] += gaps;
                shared.windowTimestampGaps[window][index] += gaps;
            }
            if (shared.captureIdBaseline[index] && frame.captureId > shared.lastCaptureId[index] + 1) {
                const auto gaps = frame.captureId - shared.lastCaptureId[index] - 1;
                shared.captureIdGaps[index] += gaps;
                shared.windowCaptureIdGaps[window][index] += gaps;
            }
            shared.lastTimestamp[index] = frame.timestamp;
            shared.lastCaptureId[index] = frame.captureId;
            shared.captureIdBaseline[index] = true;
            if (internal && shared.internalBaseline[index] && internalCount > shared.lastInternalCount[index] + 1) {
                const auto gaps = internalCount - shared.lastInternalCount[index] - 1;
                shared.internalGaps[index] += gaps;
                shared.windowInternalGaps[window][index] += gaps;
            }
            if (internal) {
                shared.lastInternalCount[index] = internalCount;
                shared.internalBaseline[index] = 1;
            }
            overflow = shared.queues[index].size() >= kMaxQueued;
            if (overflow) {
                ++shared.queueDrops[index];
                ++shared.windowQueueDrops[window][index];
            }
            else
                shared.queues[index].push_back(frame);
        }
        shared.ready.notify_one();
        if (overflow && cam.output->releaseBuffer(buffer) != STATUS_OK) {
            shared.failed = true;
            break;
        }
    }
    {
        std::lock_guard<std::mutex> guard(shared.mutex);
        shared.exited[index] = true;
    }
    shared.ready.notify_one();
}

std::vector<uint8_t> readY(const Slot* slot)
{
    check(slot && slot->image != EGL_NO_IMAGE_KHR, "EGLImage missing for pixel validation");
    CUgraphicsResource resource = nullptr;
    cudaCheck(cudaFree(nullptr), "initialize CUDA context on validation thread");
    const auto registered = cuGraphicsEGLRegisterImage(&resource, slot->image, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
    if (registered != CUDA_SUCCESS) {
        const char* name = nullptr;
        cuGetErrorName(registered, &name);
        throw std::runtime_error(std::string("CUDA EGL registration failed: ") + (name ? name : "unknown"));
    }
    CUeglFrame frame{};
    const auto mapped = cuGraphicsResourceGetMappedEglFrame(&frame, resource, 0, 0);
    std::vector<uint8_t> pixels(kWidth * kHeight);
    CUDA_MEMCPY2D transfer{};
    if (mapped == CUDA_SUCCESS && frame.frameType == CU_EGL_FRAME_TYPE_PITCH &&
        frame.width >= kWidth && frame.height >= kHeight && frame.pitch >= kWidth) {
        transfer.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        transfer.srcDevice = reinterpret_cast<CUdeviceptr>(frame.frame.pPitch[0]);
        transfer.srcPitch = frame.pitch;
        transfer.dstMemoryType = CU_MEMORYTYPE_HOST;
        transfer.dstHost = pixels.data();
        transfer.dstPitch = kWidth;
        transfer.WidthInBytes = kWidth;
        transfer.Height = kHeight;
    }
    const auto copied = transfer.Height ? cuMemcpy2D(&transfer) : CUDA_ERROR_INVALID_VALUE;
    const auto unregistered = cuGraphicsUnregisterResource(resource);
    check(mapped == CUDA_SUCCESS && copied == CUDA_SUCCESS && unregistered == CUDA_SUCCESS,
          "CUDA EGL Y-plane transfer failed");
    return pixels;
}

uint64_t checksum(const std::vector<uint8_t>& pixels)
{
    uint64_t sum = 14695981039346656037ULL;
    bool sawNonzero = false;
    for (uint32_t row = 0; row < kHeight; row += 32)
        for (uint32_t col = 0; col < kWidth; col += 32) {
            const uint8_t pixel = pixels[static_cast<size_t>(row) * kWidth + col];
            sawNonzero |= pixel != 0;
            sum = (sum ^ pixel) * 1099511628211ULL;
        }
    check(sawNonzero && sum != 0, "zero Y-plane sample or checksum");
    return sum;
}

void savePng(const std::vector<uint8_t>& pixels, size_t index)
{
    const std::string path = "/home/sst/vision-p2d-cam" + std::to_string(index) + ".png";
    FILE* file = std::fopen(path.c_str(), "wb");
    check(file != nullptr, "cannot open Y-plane PNG");
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_write_struct(&png, &info);
        std::fclose(file);
        throw std::runtime_error("Y-plane PNG write failed");
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, kWidth, kHeight, 8, PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (size_t y = 0; y < kHeight; ++y)
        png_write_row(png, const_cast<png_bytep>(pixels.data() + y * kWidth));
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    check(std::fclose(file) == 0, "Y-plane PNG close failed");
    std::printf("y_plane_png sensor=%zu path=%s\n", index, path.c_str());
}

uint64_t memAvailableKiB()
{
    FILE* file = std::fopen("/proc/meminfo", "r");
    check(file != nullptr, "cannot open /proc/meminfo");
    char line[256];
    uint64_t value = 0;
    while (std::fgets(line, sizeof(line), file)) {
        unsigned long long kib = 0;
        if (std::sscanf(line, "MemAvailable: %llu kB", &kib) == 1) {
            value = kib;
            break;
        }
    }
    std::fclose(file);
    check(value != 0, "MemAvailable missing from /proc/meminfo");
    return value;
}

void releasePair(const std::shared_ptr<Pair>& pair, std::array<Camera, 2>& cameras)
{
    Status status[2];
    for (size_t i = 0; i < 2; ++i)
        status[i] = cameras[i].output->releaseBuffer(pair->frames[i].buffer);
    check(status[0] == STATUS_OK && status[1] == STATUS_OK, "ring buffer release failed");
}

double percentile(const std::vector<double>& sorted, double p)
{
    if (sorted.empty())
        return 0;
    return sorted[static_cast<size_t>(std::ceil(p * sorted.size())) - 1];
}

std::array<double, 3> summary(std::vector<double> values)
{
    if (values.empty()) return {0, 0, 0};
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    return {values.front(), (values[(n - 1) / 2] + values[n / 2]) / 2, values.back()};
}

double pairCoverage(uint64_t pairs, uint64_t left, uint64_t right)
{
    const uint64_t potential = std::min(left, right);
    return potential ? 100.0 * pairs / potential : 0;
}

void printMetadataSummary(size_t camera, const std::vector<MetadataSample>& samples)
{
    std::array<std::vector<double>, 5> values;
    uint64_t hq = 0, fast = 0, off = 0, unknown = 0;
    for (const auto& sample : samples) {
        values[0].push_back(sample.analogGain);
        values[1].push_back(sample.ispDigitalGain);
        values[2].push_back(sample.exposureMs);
        values[3].push_back(sample.frameMs);
        values[4].push_back(sample.denoiseStrength);
        if (sample.denoiseMode == DENOISE_MODE_HIGH_QUALITY) ++hq;
        else if (sample.denoiseMode == DENOISE_MODE_FAST) ++fast;
        else if (sample.denoiseMode == DENOISE_MODE_OFF) ++off;
        else ++unknown;
    }
    const char* names[5] = {"analog_gain", "isp_digital_gain", "exposure_ms",
                            "frame_duration_ms", "denoise_strength"};
    std::printf("metadata_summary sensor=%zu samples=%zu applied_denoise_mode_hq=%llu fast=%llu off=%llu unknown=%llu\n",
                camera, samples.size(), static_cast<unsigned long long>(hq),
                static_cast<unsigned long long>(fast), static_cast<unsigned long long>(off),
                static_cast<unsigned long long>(unknown));
    for (size_t field = 0; field < values.size(); ++field) {
        const auto stats = summary(std::move(values[field]));
        std::printf("metadata_summary sensor=%zu %s_min=%.4f median=%.4f max=%.4f\n",
                    camera, names[field], stats[0], stats[1], stats[2]);
    }
}

uint64_t distanceNs(uint64_t a, uint64_t b)
{
    return a > b ? a - b : b - a;
}

int pairAction(const std::deque<Frame>& left, const std::deque<Frame>& right)
{
    if (left.empty() || right.empty())
        return -1;
    const uint64_t a = left.front().timestamp;
    const uint64_t b = right.front().timestamp;
    if (a > b && right.size() > 1 &&
        distanceNs(a, right[1].timestamp) < distanceNs(a, b))
        return 1;
    if (b > a && left.size() > 1 &&
        distanceNs(b, left[1].timestamp) < distanceNs(b, a))
        return 0;
    if (distanceNs(a, b) <= kPairThresholdNs)
        return 2;
    return a < b ? 0 : 1;
}

#ifdef PROBE_SELF_TEST
int main()
{
    const auto frame = [](uint64_t ts) { return Frame{nullptr, ts, 0}; };
    check(pairAction({frame(100000000)}, {frame(118000000)}) == 2, "18ms pair rejected");
    check(pairAction({frame(100000000)}, {frame(118666667)}) == 2, "threshold pair rejected");
    check(pairAction({frame(100000000)}, {frame(118666668)}) == 0, "over-threshold pair accepted");
    check(pairAction({frame(110000000)}, {frame(100000000), frame(110100000)}) == 1,
          "nearer right frame not selected");
    check(pairAction({frame(100000000), frame(110100000)}, {frame(110000000)}) == 0,
          "nearer left frame not selected");
    check(pairAction({}, {frame(1)}) == -1, "empty queue paired");
    check(percentile({1, 2, 3, 4}, .95) == 4, "p95 incorrect");
    check(summary({4, 1, 9}) == std::array<double, 3>{1, 4, 9}, "min/median/max incorrect");
    check(pairCoverage(90, 100, 100) == 90.0, "90 percent incorrectly passes pairing");
    check(pairCoverage(95, 100, 110) == 95.0, "potential pair denominator incorrect");
    check(pairCoverage(0, 0, 0) == 0.0, "empty pairing passed");
    uint64_t duration = 0;
    check(selectFrameDuration(Range<uint64_t>(33333334, 33333334), &duration) &&
          duration == 33333334, "rounded 30fps minimum rejected");
    check(selectFrameDuration(Range<uint64_t>(33333333, 40000000), &duration) &&
          duration == kFrameNs, "nominal 30fps duration not selected");
    check(selectFrameDuration(Range<uint64_t>(33000000, 33333332), &duration) &&
          duration == 33333332, "nominal duration not clamped to maximum");
    check(selectFrameDuration(Range<uint64_t>(33366667, 40000000), &duration),
           "mode within 0.1% of 30fps rejected");
    check(!selectFrameDuration(Range<uint64_t>(33366668, 40000000), &duration),
           "mode outside 0.1% of 30fps selected");
    check(windowFor(std::chrono::seconds(4)) == 0 && windowFor(std::chrono::seconds(5)) == 1,
           "startup boundary incorrect");
    check(!validationPassed(1, 1, 1, 2) && validationPassed(1, 0, 1, 2),
           "validation failure must not abort capture but must fail acceptance");
    std::puts("pairing, statistics and mode-selection self-test passed");
}
#else

int run()
{
    UniqueObj<CameraProvider> provider(CameraProvider::create());
    auto* api = interface_cast<ICameraProvider>(provider);
    check(api != nullptr, "Argus provider unavailable");
    std::vector<CameraDevice*> devices;
    const bool internalCountAvailable = api->supportsExtension(EXT_INTERNAL_FRAME_COUNT);
    std::printf("internal_frame_count_extension=%s\n", internalCountAvailable ? "yes" : "no");
    check(api->getCameraDevices(&devices) == STATUS_OK && devices.size() >= 2,
          "sensors 0 and 1 unavailable");
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    check(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr) == EGL_TRUE,
          "EGL display initialization failed");
    struct DisplayGuard {
        EGLDisplay display;
        ~DisplayGuard() { eglTerminate(display); }
    } displayGuard{display};
    {
        std::array<Camera, 2> cameras;
        makeCamera(cameras[0], api, devices[0], 0, display);
        makeCamera(cameras[1], api, devices[1], 1, display);
        struct CudaGuard {
            void* pointer = nullptr;
            ~CudaGuard() { if (pointer) cudaFree(pointer); }
        } pressure;
        cudaCheck(cudaMalloc(&pressure.pointer, 1536ULL * 1024 * 1024), "1.5 GiB CUDA allocation failed");
        cudaCheck(cudaMemset(pressure.pointer, 0, 1536ULL * 1024 * 1024), "1.5 GiB CUDA initialization failed");
        std::atomic<bool> gpuStop{false};
        std::atomic<bool> gpuFailed{false};
        std::thread gpu;
        Shared shared;
        std::array<std::thread, 2> consumers;
        std::mutex sampleMutex;
        std::condition_variable sampleReady;
         struct SampleJob { std::shared_ptr<Pair> pair; bool recheck; bool snapshot; };
        std::deque<SampleJob> sampleQueue;
        bool sampleStop = false;
        std::atomic<bool> sampleFailed{false};
         std::atomic<uint64_t> checksumsValid{0};
         std::atomic<uint64_t> checksumsFailed{0};
         std::atomic<uint64_t> snapshotsSaved{0};
         std::atomic<uint64_t> tenureValidated{0};
        std::thread sampler;
        std::deque<std::shared_ptr<Pair>> ring;
        std::deque<std::shared_ptr<Pair>> deferred;
         std::vector<double> skews;
         std::vector<double> steadySkews;
         std::vector<double> candidateSkews;
         uint64_t steadyPairs = 0;
        uint64_t unmatched[2]{};
        uint64_t pairs = 0;
        uint64_t afterFull = 0;
        uint64_t sampledScheduled = 0;
         uint64_t tenureScheduled = 0;
         bool snapshotScheduled = false;
         size_t maxHeld = 0;
        uint64_t minAvailable = std::numeric_limits<uint64_t>::max();
         auto started = Clock::now();
         shared.started = started;
         auto deadline = started + std::chrono::minutes(5);
        auto report = started + std::chrono::seconds(1);
        std::exception_ptr runError;
         auto enqueueSample = [&](const std::shared_ptr<Pair>& pair, bool recheck, bool snapshot = false) {
            ++pair->pendingSamples;
            {
                std::lock_guard<std::mutex> lock(sampleMutex);
                 sampleQueue.push_back({pair, recheck, snapshot});
            }
            sampleReady.notify_one();
        };
        try {
            gpu = std::thread([&] {
                while (!gpuStop.load()) {
                    if (cudaMemset(pressure.pointer, 0x5a, 8 * 1024 * 1024) != cudaSuccess ||
                        cudaDeviceSynchronize() != cudaSuccess) {
                        gpuFailed = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
            sampler = std::thread([&] {
                while (true) {
                    SampleJob job;
                    {
                        std::unique_lock<std::mutex> lock(sampleMutex);
                        sampleReady.wait(lock, [&] { return sampleStop || !sampleQueue.empty(); });
                        if (sampleQueue.empty())
                            break;
                        job = sampleQueue.front();
                        sampleQueue.pop_front();
                    }
                    try {
                        auto& pair = job.pair;
                        const auto* left = static_cast<const Slot*>(interface_cast<IBuffer>(pair->frames[0].buffer)->getClientData());
                        const auto* right = static_cast<const Slot*>(interface_cast<IBuffer>(pair->frames[1].buffer)->getClientData());
                         const auto leftY = readY(left);
                         const auto rightY = readY(right);
                         const std::array<uint64_t, 2> hashes{checksum(leftY), checksum(rightY)};
                         if (job.snapshot) {
                             savePng(leftY, 0);
                             ++snapshotsSaved;
                             savePng(rightY, 1);
                             ++snapshotsSaved;
                         }
                        if (job.recheck) {
                            check(pair->sampled && hashes == pair->firstChecksum,
                                  "checksum changed while pair held in ring");
                            ++tenureValidated;
                        } else {
                            pair->firstChecksum = hashes;
                            pair->sampled = true;
                            ++checksumsValid;
                        }
                        std::printf("checksum pair_ts=%llu,%llu stage=%s y_sample_fnv=%016llx,%016llx\n",
                                    static_cast<unsigned long long>(pair->frames[0].timestamp),
                                    static_cast<unsigned long long>(pair->frames[1].timestamp),
                                    job.recheck ? "ring_exit" : "acquire",
                                    static_cast<unsigned long long>(hashes[0]),
                                    static_cast<unsigned long long>(hashes[1]));
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "checksum failed: %s\n", e.what());
                         sampleFailed = true;
                         ++checksumsFailed;
                    }
                    --job.pair->pendingSamples;
                }
            });
            for (size_t i = 0; i < 2; ++i)
                consumers[i] = std::thread(consume, std::ref(cameras[i]), std::ref(shared), i, internalCountAvailable);
            for (auto& cam : cameras)
                check(cam.capture->repeat(cam.request.get()) == STATUS_OK, "repeat capture failed");
            minAvailable = memAvailableKiB();
             while (Clock::now() < deadline && !shared.failed) {
                std::array<Frame, 2> frames{};
                int action = -1;
                {
                    std::unique_lock<std::mutex> lock(shared.mutex);
                    shared.ready.wait_until(lock, std::min(report, deadline), [&] {
                        return shared.failed || shared.exited[0] || shared.exited[1] ||
                               (!shared.queues[0].empty() && !shared.queues[1].empty());
                    });
                    if (shared.exited[0] || shared.exited[1])
                        shared.failed = true;
                    if (!shared.failed) {
                        action = pairAction(shared.queues[0], shared.queues[1]);
                        if (action >= 0)
                            candidateSkews.push_back(distanceNs(shared.queues[0].front().timestamp,
                                                                  shared.queues[1].front().timestamp) / 1000.0);
                        if (action == 2) {
                            frames = {shared.queues[0].front(), shared.queues[1].front()};
                            shared.queues[0].pop_front();
                            shared.queues[1].pop_front();
                        } else if (action >= 0) {
                            frames[action] = shared.queues[action].front();
                            shared.queues[action].pop_front();
                            ++unmatched[action];
                        }
                    }
                }
                if (action == 2) {
                    auto pair = std::make_shared<Pair>();
                    pair->frames = frames;
                    double skewUs = distanceNs(frames[0].timestamp, frames[1].timestamp) / 1000.0;
                    skews.push_back(skewUs);
                    if (Clock::now() - started >= std::chrono::seconds(5)) {
                        steadySkews.push_back(skewUs);
                        ++steadyPairs;
                    }
                    std::printf("pair=%llu sensor_ts_ns=%llu,%llu skew_us=%.3f\n",
                                static_cast<unsigned long long>(++pairs),
                                static_cast<unsigned long long>(frames[0].timestamp),
                                static_cast<unsigned long long>(frames[1].timestamp), skewUs);
                    if (ring.size() == kHeldPairs) {
                        auto oldest = ring.front();
                        if (oldest->selected && !sampleFailed) {
                            if (!oldest->pendingSamples) {
                                enqueueSample(oldest, true);
                                oldest->recheckQueued = true;
                                ++tenureScheduled;
                            }
                            deferred.push_back(oldest);
                        } else if (oldest->pendingSamples) {
                            deferred.push_back(oldest);
                        } else {
                            releasePair(oldest, cameras);
                        }
                        ring.pop_front();
                        ++afterFull;
                    }
                     ring.push_back(pair);
                     maxHeld = std::max(maxHeld, ring.size());
                     const bool snapshot = !snapshotScheduled && Clock::now() - started >= std::chrono::seconds(60);
                     if (pairs % 30 == 0 || snapshot) {
                         pair->selected = true;
                         enqueueSample(pair, false, snapshot);
                         ++sampledScheduled;
                         if (snapshot) snapshotScheduled = true;
                     }
                } else if (action >= 0) {
                    check(cameras[action].output->releaseBuffer(frames[action].buffer) == STATUS_OK,
                          "unmatched buffer release failed");
                }
                for (auto it = deferred.begin(); it != deferred.end();) {
                    if (!(*it)->pendingSamples) {
                        if ((*it)->selected && !(*it)->recheckQueued && (*it)->sampled && !sampleFailed) {
                            enqueueSample(*it, true);
                            (*it)->recheckQueued = true;
                            ++tenureScheduled;
                            ++it;
                        } else {
                            releasePair(*it, cameras);
                            it = deferred.erase(it);
                        }
                    } else {
                        ++it;
                    }
                }
                if (Clock::now() >= report) {
                    minAvailable = std::min(minAvailable, memAvailableKiB());
                    std::lock_guard<std::mutex> lock(shared.mutex);
                    for (size_t index = 0; index < 2; ++index) {
                        if (!shared.metadataSamples[index].empty()) {
                            const auto& sample = shared.metadataSamples[index].back();
                            std::printf("metadata sensor=%zu capture_id=%u sensor_ts_ns=%llu analog_gain=%.4f isp_digital_gain=%.4f exposure_ms=%.4f frame_duration_ms=%.4f denoise_mode=%s denoise_strength=%.4f source=capture_metadata\n",
                                        index, sample.captureId, static_cast<unsigned long long>(sample.timestamp),
                                        sample.analogGain, sample.ispDigitalGain, sample.exposureMs,
                                        sample.frameMs, denoiseName(sample.denoiseMode), sample.denoiseStrength);
                        }
                    }
                    const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
                    std::printf("elapsed_s=%.1f fps=%.2f,%.2f received=%llu,%llu timestamp_gaps=%llu,%llu reordered_ts=%llu,%llu capture_id_gaps=%llu,%llu queue_drops=%llu,%llu internal_gaps=%llu,%llu acquire_timeouts=%llu,%llu unmatched=%llu,%llu pairs=%llu held=%zu continued_after_45=%llu cuda_reserved_mib=1536 gpu_load=cudaMemset+sync mem_available_min_kib=%llu\n",
                                seconds, shared.received[0] / seconds, shared.received[1] / seconds,
                                static_cast<unsigned long long>(shared.received[0]),
                                static_cast<unsigned long long>(shared.received[1]),
                                static_cast<unsigned long long>(shared.timestampGaps[0]),
                                static_cast<unsigned long long>(shared.timestampGaps[1]),
                                static_cast<unsigned long long>(shared.reorderedTimestamps[0]),
                                static_cast<unsigned long long>(shared.reorderedTimestamps[1]),
                                static_cast<unsigned long long>(shared.captureIdGaps[0]),
                                static_cast<unsigned long long>(shared.captureIdGaps[1]),
                                static_cast<unsigned long long>(shared.queueDrops[0]),
                                static_cast<unsigned long long>(shared.queueDrops[1]),
                                static_cast<unsigned long long>(shared.internalGaps[0]),
                                static_cast<unsigned long long>(shared.internalGaps[1]),
                                static_cast<unsigned long long>(shared.acquisitionTimeouts[0]),
                                static_cast<unsigned long long>(shared.acquisitionTimeouts[1]),
                                static_cast<unsigned long long>(unmatched[0]),
                                static_cast<unsigned long long>(unmatched[1]),
                                static_cast<unsigned long long>(pairs), ring.size(),
                                static_cast<unsigned long long>(afterFull),
                                static_cast<unsigned long long>(minAvailable));
                    report = Clock::now() + std::chrono::seconds(1);
                }
            }
        } catch (...) {
            runError = std::current_exception();
        }
        const auto stopped = Clock::now();
        shared.stop = true;
        for (auto& cam : cameras) {
            if (cam.capture) cam.capture->stopRepeat();
            if (cam.output) cam.output->endOfStream();
        }
        for (auto& thread : consumers)
            if (thread.joinable()) thread.join();
        {
            std::lock_guard<std::mutex> lock(sampleMutex);
            sampleStop = true;
        }
        sampleReady.notify_one();
        if (sampler.joinable()) sampler.join();
        gpuStop = true;
        if (gpu.joinable()) gpu.join();
        try {
            minAvailable = std::min(minAvailable, memAvailableKiB());
        } catch (...) {
            if (!runError) runError = std::current_exception();
        }
        for (size_t index = 0; index < 2; ++index)
            for (auto& frame : shared.queues[index])
                if (cameras[index].output->releaseBuffer(frame.buffer) != STATUS_OK && !runError)
                    runError = std::make_exception_ptr(std::runtime_error("queued buffer release failed"));
        for (const auto& pair : deferred) {
            try { releasePair(pair, cameras); }
            catch (...) { if (!runError) runError = std::current_exception(); }
        }
        for (const auto& pair : ring) {
            try { releasePair(pair, cameras); }
            catch (...) { if (!runError) runError = std::current_exception(); }
        }
        const double seconds = std::chrono::duration<double>(stopped - started).count();
        const double steadySeconds = std::max(0.0, seconds - 5.0);
        for (size_t window = 0; window < 2; ++window) {
            const double duration = window == 0 ? std::min(seconds, 5.0) : steadySeconds;
            std::printf("window=%s elapsed_s=%.3f received=%llu,%llu fps=%.3f,%.3f timestamp_gaps=%llu,%llu acquire_errors=%llu,%llu queue_drops=%llu,%llu capture_id_gaps=%llu,%llu internal_gaps=%llu,%llu reordered=%llu,%llu\n",
                        window == 0 ? "startup" : "steady", duration,
                        static_cast<unsigned long long>(shared.windowReceived[window][0]),
                        static_cast<unsigned long long>(shared.windowReceived[window][1]),
                        duration ? shared.windowReceived[window][0] / duration : 0,
                        duration ? shared.windowReceived[window][1] / duration : 0,
                        static_cast<unsigned long long>(shared.windowTimestampGaps[window][0]),
                        static_cast<unsigned long long>(shared.windowTimestampGaps[window][1]),
                        static_cast<unsigned long long>(shared.windowAcquireErrors[window][0]),
                        static_cast<unsigned long long>(shared.windowAcquireErrors[window][1]),
                        static_cast<unsigned long long>(shared.windowQueueDrops[window][0]),
                        static_cast<unsigned long long>(shared.windowQueueDrops[window][1]),
                        static_cast<unsigned long long>(shared.windowCaptureIdGaps[window][0]),
                        static_cast<unsigned long long>(shared.windowCaptureIdGaps[window][1]),
                        static_cast<unsigned long long>(shared.windowInternalGaps[window][0]),
                        static_cast<unsigned long long>(shared.windowInternalGaps[window][1]),
                        static_cast<unsigned long long>(shared.windowReordered[window][0]),
                        static_cast<unsigned long long>(shared.windowReordered[window][1]));
        }
        std::sort(skews.begin(), skews.end());
        std::sort(steadySkews.begin(), steadySkews.end());
        std::sort(candidateSkews.begin(), candidateSkews.end());
        const uint64_t candidates = std::min(shared.received[0], shared.received[1]);
         const double pairedPct = pairCoverage(steadyPairs, shared.windowReceived[1][0], shared.windowReceived[1][1]);
        std::printf("candidate_skew_us observations=%zu p50=%.3f p95=%.3f p99=%.3f unobserved_potential=%llu\n",
                    candidateSkews.size(), percentile(candidateSkews, .50),
                    percentile(candidateSkews, .95), percentile(candidateSkews, .99),
                    static_cast<unsigned long long>(candidates > pairs ? candidates - pairs : 0));
         const double fps[2] = {steadySeconds ? shared.windowReceived[1][0] / steadySeconds : 0,
                                steadySeconds ? shared.windowReceived[1][1] / steadySeconds : 0};
         const bool noDrops = shared.windowTimestampGaps[1][0] == 0 && shared.windowTimestampGaps[1][1] == 0 &&
             shared.windowCaptureIdGaps[1][0] == 0 && shared.windowCaptureIdGaps[1][1] == 0 &&
             shared.windowQueueDrops[1][0] == 0 && shared.windowQueueDrops[1][1] == 0 &&
             shared.windowAcquireErrors[1][0] == 0 && shared.windowAcquireErrors[1][1] == 0 &&
             shared.windowReordered[1][0] == 0 && shared.windowReordered[1][1] == 0 &&
             (!internalCountAvailable || (shared.windowInternalGaps[1][0] == 0 && shared.windowInternalGaps[1][1] == 0));
        for (size_t index = 0; index < 2; ++index)
            printMetadataSummary(index, shared.metadataSamples[index]);
        const bool metadataAdequate = shared.metadataSamples[0].size() >= 290 &&
            shared.metadataSamples[1].size() >= 290;
         const bool pass = !runError && !shared.failed && !gpuFailed &&
             seconds >= 300 && fps[0] >= 29.5 && fps[1] >= 29.5 && noDrops &&
             metadataAdequate && pairedPct >= 95.0 && maxHeld >= kHeldPairs && afterFull > 0 &&
             checksumsValid == sampledScheduled && tenureValidated == tenureScheduled &&
             validationPassed(checksumsValid, checksumsFailed, tenureValidated, snapshotsSaved) &&
             minAvailable >= kMinAvailableKiB;
        std::printf("final elapsed_s=%.3f fps=%.3f,%.3f received=%llu,%llu paired=%llu steady_paired=%llu steady_min_received=%llu paired_pct=%.3f threshold_us=18666.667 skew_us_p50=%.3f p95=%.3f p99=%.3f timestamp_gaps=%llu,%llu capture_id_gaps=%llu,%llu queue_drops=%llu,%llu internal_gaps=%llu,%llu acquire_timeouts=%llu,%llu reordered_ts=%llu,%llu unmatched=%llu,%llu checksums=%llu/%llu tenure_rechecks=%llu/%llu held_max=%zu continued_after_45=%llu validation_failures=%llu snapshots_saved=%llu mem_available_min_kib=%llu mem_available_limit_kib=%llu cuda_reserved_mib=1536 gpu_load=cudaMemset+sync acceptance=%s\n",
                    seconds, fps[0], fps[1],
                    static_cast<unsigned long long>(shared.received[0]),
                    static_cast<unsigned long long>(shared.received[1]),
                     static_cast<unsigned long long>(pairs), static_cast<unsigned long long>(steadyPairs),
                     static_cast<unsigned long long>(std::min(shared.windowReceived[1][0], shared.windowReceived[1][1])), pairedPct,
                    percentile(steadySkews, .50), percentile(steadySkews, .95), percentile(steadySkews, .99),
                    static_cast<unsigned long long>(shared.timestampGaps[0]),
                    static_cast<unsigned long long>(shared.timestampGaps[1]),
                    static_cast<unsigned long long>(shared.captureIdGaps[0]),
                    static_cast<unsigned long long>(shared.captureIdGaps[1]),
                    static_cast<unsigned long long>(shared.queueDrops[0]),
                    static_cast<unsigned long long>(shared.queueDrops[1]),
                    static_cast<unsigned long long>(shared.internalGaps[0]),
                    static_cast<unsigned long long>(shared.internalGaps[1]),
                    static_cast<unsigned long long>(shared.acquisitionTimeouts[0]),
                    static_cast<unsigned long long>(shared.acquisitionTimeouts[1]),
                    static_cast<unsigned long long>(shared.reorderedTimestamps[0]),
                    static_cast<unsigned long long>(shared.reorderedTimestamps[1]),
                    static_cast<unsigned long long>(unmatched[0]),
                    static_cast<unsigned long long>(unmatched[1]),
                    static_cast<unsigned long long>(checksumsValid.load()),
                    static_cast<unsigned long long>(sampledScheduled),
                    static_cast<unsigned long long>(tenureValidated.load()),
                     static_cast<unsigned long long>(tenureScheduled), maxHeld,
                     static_cast<unsigned long long>(afterFull),
                     static_cast<unsigned long long>(checksumsFailed.load()),
                     static_cast<unsigned long long>(snapshotsSaved.load()),
                     static_cast<unsigned long long>(minAvailable),
                    static_cast<unsigned long long>(kMinAvailableKiB), pass ? "PASS" : "FAIL");
        if (runError) std::rethrow_exception(runError);
        check(pass, "five-minute capture acceptance failed; inspect final counters");
    }
    return 0;
}

int main()
{
    try {
        return run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "probe failed: %s\n", e.what());
        return 1;
    }
}
#endif
