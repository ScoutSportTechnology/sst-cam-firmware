#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>
using Clock = std::chrono::steady_clock;

constexpr uint64_t kPairThresholdNs = 18666667;
constexpr int kStartupSeconds = 5;
bool noDropVerdict(uint64_t timestampGaps, uint64_t captureGaps, uint64_t internalGaps,
                   uint64_t reordered, uint64_t queueDrops, uint64_t unmatched,
                   uint64_t renderMisses) {
    return !(timestampGaps || captureGaps || internalGaps || reordered || queueDrops ||
             unmatched || renderMisses);
}
// Detection is scheduled at exactly 15 Hz, so a window can hold one batch fewer than 15 x seconds.
bool inferenceKeptUp(uint64_t batches, uint64_t skipped, double seconds) {
    return !skipped && batches + 1 >= 15.0 * seconds;
}
bool steadyAcceptance(bool noCaptureDrops, double leftFps, double rightFps,
                      bool inferenceOk, double renderFps, double encodedFps) {
    return noCaptureDrops && leftFps >= 29.5 && rightFps >= 29.5 &&
           inferenceOk && renderFps >= 29.5 && encodedFps >= 29.5;
}
size_t windowFor(Clock::duration elapsed) {
    return elapsed < std::chrono::seconds(kStartupSeconds) ? 0 : 1;
}
double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>(std::ceil(fraction * values.size())) - 1];
}
float panForTick(uint64_t tick) { return .5f + .5f * std::sin(tick * .015f); }
float zoomForTick(uint64_t tick) { return 2.f - std::cos(tick * 3.141592653589793 / 1500.); }
// Shared with the NVRTC kernel so the host test checks the renderer's visibility decision.
#define E2E_VISIBILITY(qualifier) \
    struct Visibility { float weight; bool left; bool right; }; \
    qualifier Visibility visibility(float az, float yaw0, float yaw1) { \
        float overlap = 4.f * .01745329252f; \
        float limit = 38.f * .01745329252f; \
        float weight = fminf(1.f, fmaxf(0.f, (az + overlap) / (2.f * overlap))); \
        if (fabsf(az - yaw0) > limit && fabsf(az - yaw1) <= limit) weight = 1.f; \
        if (fabsf(az - yaw1) > limit && fabsf(az - yaw0) <= limit) weight = 0.f; \
        return {weight, weight < 1.f, weight > 0.f}; \
    }
E2E_VISIBILITY()
#define E2E_STRINGIFY_INNER(...) #__VA_ARGS__
#define E2E_STRINGIFY(...) E2E_STRINGIFY_INNER(__VA_ARGS__)
struct Ray { float yaw; };
Ray projectRay(float x, float width, int camera) {
    return {static_cast<float>(std::atan((x - width / 2) / (width * .5)) * 180 / 3.141592653589793 +
                               (camera ? 36. : -36.))};
}
uint64_t presentationNs(uint64_t timestamp, uint64_t first) {
    if (timestamp < first) throw std::runtime_error("sensor timestamps reversed");
    return timestamp - first;
}
template <typename Frame>
int pairAction(const std::deque<Frame>& left, const std::deque<Frame>& right) {
    if (left.empty() || right.empty()) return -1;
    const auto a = left.front().ts, b = right.front().ts;
    const auto distance = [](uint64_t x, uint64_t y) { return x > y ? x - y : y - x; };
    if (a > b && right.size() > 1 && distance(a, right[1].ts) < distance(a, b)) return 1;
    if (b > a && left.size() > 1 && distance(b, left[1].ts) < distance(a, b)) return 0;
    if (distance(a, b) <= kPairThresholdNs) return 2;
    return a < b ? 0 : 1;
}

#ifdef E2E_SELF_TEST
#include <cassert>
#include <cstdio>
struct TestFrame { uint64_t ts; };
int main() {
    assert(pairAction(std::deque<TestFrame>{{100}}, std::deque<TestFrame>{{100}, {130}}) == 2);
    assert(pairAction(std::deque<TestFrame>{{100}}, std::deque<TestFrame>{{20000000}}) == 0);
    assert(pairAction(std::deque<TestFrame>{{20000000}}, std::deque<TestFrame>{{100}}) == 1);
    assert(pairAction(std::deque<TestFrame>{{110}}, std::deque<TestFrame>{{100}, {111}}) == 1);
    assert(pairAction(std::deque<TestFrame>{}, std::deque<TestFrame>{{100}}) == -1);
    assert(presentationNs(1033333333, 1000000000) == 33333333);
    assert(panForTick(0) >= 0.f && panForTick(0) <= 1.f);
    for (uint64_t tick = 0; tick < 1000; ++tick)
        assert(panForTick(tick) >= 0.f && panForTick(tick) <= 1.f);
    bool reversed = false;
    try { presentationNs(1, 2); } catch (const std::runtime_error&) { reversed = true; }
    assert(reversed);
    assert(windowFor(std::chrono::seconds(4)) == 0);
    assert(windowFor(std::chrono::seconds(5)) == 1);
    assert(percentile({1, 2, 3, 4}, .99) == 4);
    assert(percentile({}, .99) == 0);
    assert(std::abs(zoomForTick(0) - 1.f) < .001f);
    assert(zoomForTick(1500) > 2.9f);
    assert(std::abs(zoomForTick(3000) - 1.f) < .001f);
    assert(std::abs(projectRay(960.f, 1920.f, 0).yaw + 36.f) < .01f);
    assert(std::abs(projectRay(960.f, 1920.f, 1).yaw - 36.f) < .01f);
    const float yaw0 = -36.f * .01745329252f, yaw1 = 36.f * .01745329252f;
    const auto edge = visibility(std::atan2(1.5f, 1.f), yaw0, yaw1);
    assert(!edge.left && edge.right && edge.weight == 1.f);
    const auto otherEdge = visibility(std::atan2(-1.5f, 1.f), yaw0, yaw1);
    assert(otherEdge.left && !otherEdge.right && otherEdge.weight == 0.f);
    const auto center = visibility(0.f, yaw0, yaw1);
    assert(center.left && center.right && std::abs(center.weight - .5f) < .001f);
    const auto feather = visibility(.02f, yaw0, yaw1);
    assert(feather.left && feather.right && feather.weight > .5f && feather.weight < 1.f);
    assert(!steadyAcceptance(true, 30., 30., true, 30., 29.4));
    assert(!noDropVerdict(0, 0, 0, 0, 0, 0, 1));
    assert(noDropVerdict(0, 0, 0, 0, 0, 0, 0));
    assert(steadyAcceptance(true, 30.0, 30.0, true, 29.5, 29.5));
    assert(!steadyAcceptance(false, 30.0, 30.0, true, 30.0, 30.0));
    assert(!steadyAcceptance(true, 29.4, 30.0, true, 30.0, 30.0));
    assert(!steadyAcceptance(true, 30.0, 30.0, false, 30.0, 30.0));
    assert(!steadyAcceptance(true, 30.0, 30.0, true, 29.49, 30.0));
    assert(!steadyAcceptance(true, 30.0, 30.0, true, 30.0, 29.49));
    // Session 2 4K steady window: 9150 batches in 610.027 s with no skipped slot.
    assert(inferenceKeptUp(9150, 0, 610.027));
    assert(!inferenceKeptUp(9149, 0, 610.027));
    assert(!inferenceKeptUp(9150, 1, 610.027));
    std::puts("pair selection, window and latency statistics passed");
}
#else
#include <Argus/Argus.h>
#include <Argus/Ext/InternalFrameCount.h>
#include <cuda.h>
#include <cudaEGL.h>
#include <cuda_runtime_api.h>
#include <nvrtc.h>
#include <nvbufsurface.h>
#include <NvInfer.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/gst.h>
#include <EGL/egl.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace Argus;
using Clock = std::chrono::steady_clock;
constexpr int kOutputWidth = 1920, kOutputHeight = 1080;
constexpr size_t kBuffers = 60, kRingPairs = 45, kQueued = 4;
constexpr uint64_t kFrameNs = 1000000000ULL / 30;
constexpr int kModelSize = 512;
uint64_t distanceNs(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void cudaCheck(cudaError_t code, const char* action) {
    check(code == cudaSuccess, std::string(action) + ": " + cudaGetErrorString(code));
}
void driverCheck(CUresult code, const char* action) {
    const char* error = nullptr;
    if (code != CUDA_SUCCESS) cuGetErrorName(code, &error);
    check(code == CUDA_SUCCESS, std::string(action) + ": " + (error ? error : "unknown"));
}
void rtcCheck(nvrtcResult code, const char* action) {
    check(code == NVRTC_SUCCESS, std::string(action) + ": " + nvrtcGetErrorString(code));
}
struct Options {
    int width = 3840, height = 2160, seconds = 5400;
    std::string engine, mp4, rtmp, clip, preset = "veryfast";
};
Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i += 2) {
        check(i + 1 < argc, "missing option value");
        const std::string key(argv[i]), value(argv[i + 1]);
        if (key == "--mode") {
            check(value == "4k", "stress spike requires 4k mode");
        } else if (key == "--seconds") {
            const auto n = std::stol(value);
            check(n > 0 && n <= 5490, "seconds must be 1..5490");
            o.seconds = static_cast<int>(n);
        } else if (key == "--engine") o.engine = value;
        else if (key == "--record") o.mp4 = value;
        else if (key == "--rtmp") o.rtmp = value;
        else if (key == "--clip") o.clip = value;
        else if (key == "--preset") {
            check(value == "veryfast" || value == "faster", "preset must be veryfast or faster");
            o.preset = value;
        }
        else throw std::runtime_error("unknown option: " + key);
    }
    check(!o.clip.empty(), "decoded soccer clip required (--clip); no synthetic fallback");
    check(!o.engine.empty(), "real RF-DETR TensorRT engine required (--engine); no synthetic fallback");
    check(!o.mp4.empty() && !o.rtmp.empty(), "both --record path and --rtmp URL required");
    check(o.rtmp.rfind("rtmp://", 0) == 0 || o.rtmp.rfind("rtmps://", 0) == 0,
          "--rtmp must be an rtmp(s) URL");
    return o;
}

struct Slot {
    NvBufSurface* surface = nullptr;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    UniqueObj<Buffer> buffer;
    ~Slot() {
        buffer.reset();
        if (surface) {
            if (image != EGL_NO_IMAGE_KHR) NvBufSurfaceUnMapEglImage(surface, 0);
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
struct Frame { Buffer* buffer = nullptr; uint64_t ts = 0; };
using Pair = std::array<Frame, 2>;
struct Shared {
    std::mutex mutex;
    std::condition_variable ready;
    std::array<std::deque<Frame>, 2> queued;
    std::array<uint64_t, 2> received{}, drops{}, timeouts{}, captureGaps{}, timestampGaps{}, internalGaps{}, reordered{};
    std::array<uint32_t, 2> lastCaptureId{};
    std::array<uint64_t, 2> lastTimestamp{}, lastInternalCount{};
    std::array<bool, 2> internalBaseline{};
    bool internalAvailable = false;
    std::array<std::array<uint64_t, 2>, 2> windowReceived{}, windowDrops{}, windowCaptureGaps{}, windowTimestampGaps{}, windowInternalGaps{}, windowReordered{};
    Clock::time_point started;
    std::array<bool, 2> exited{};
    std::atomic<bool> stop{false}, failed{false};
};

void makeCamera(Camera& cam, ICameraProvider* provider, CameraDevice* device,
                EGLDisplay display, const Options& options, int index) {
    auto* props = interface_cast<ICameraProperties>(device);
    std::vector<SensorMode*> modes;
    check(props && props->getAllSensorModes(&modes) == STATUS_OK, "sensor modes unavailable");
    SensorMode* chosen = nullptr;
    uint64_t duration = 0;
    for (auto* mode : modes) {
        auto* info = interface_cast<ISensorMode>(mode);
        if (!info) continue;
        const auto resolution = info->getResolution();
        const auto range = info->getFrameDurationRange();
        if (!chosen && resolution.width() == static_cast<uint32_t>(options.width) &&
            resolution.height() == static_cast<uint32_t>(options.height) &&
            range.min() > 0 && range.min() <= 33366667ULL && range.max() >= range.min()) {
            chosen = mode;
            duration = std::max(range.min(), std::min(kFrameNs, range.max()));
        }
    }
    check(chosen != nullptr, "selected sensor mode unavailable at 30 fps");
    cam.session.reset(provider->createCaptureSession(device));
    cam.capture = interface_cast<ICaptureSession>(cam.session);
    check(cam.capture != nullptr, "Argus capture session unavailable");
    UniqueObj<OutputStreamSettings> streamSettings(cam.capture->createOutputStreamSettings(STREAM_TYPE_BUFFER));
    auto* settings = interface_cast<IBufferOutputStreamSettings>(streamSettings);
    check(settings && settings->setBufferType(BUFFER_TYPE_EGL_IMAGE) == STATUS_OK,
          "EGLImage buffer stream unavailable");
    settings->setMetadataEnable(true);
    cam.stream.reset(cam.capture->createOutputStream(streamSettings.get()));
    cam.output = interface_cast<IBufferOutputStream>(cam.stream);
    check(cam.output != nullptr, "Argus output stream unavailable");
    UniqueObj<BufferSettings> imageSettings(cam.output->createBufferSettings());
    auto* egl = interface_cast<IEGLImageBufferSettings>(imageSettings);
    check(egl && egl->setEGLDisplay(display) == STATUS_OK, "EGLImage settings unavailable");
    cam.slots.reserve(kBuffers);
    for (size_t i = 0; i < kBuffers; ++i) {
        auto slot = std::make_unique<Slot>();
        NvBufSurfaceAllocateParams params{};
        params.params.width = options.width;
        params.params.height = options.height;
        params.params.layout = NVBUF_LAYOUT_PITCH;
        params.params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
        params.params.memType = NVBUF_MEM_SURFACE_ARRAY;
        params.memtag = NvBufSurfaceTag_CAMERA;
        check(NvBufSurfaceAllocate(&slot->surface, 1, &params) == 0 && slot->surface,
              "NvBufSurface allocation failed");
        check(NvBufSurfaceMapEglImage(slot->surface, 0) == 0, "EGLImage map failed");
        slot->image = static_cast<EGLImageKHR>(slot->surface->surfaceList[0].mappedAddr.eglImage);
        check(slot->image != EGL_NO_IMAGE_KHR && egl->setEGLImage(slot->image) == STATUS_OK,
              "Argus EGLImage assignment failed");
        slot->buffer.reset(cam.output->createBuffer(imageSettings.get()));
        check(interface_cast<IBuffer>(slot->buffer) != nullptr, "Argus buffer creation failed");
        interface_cast<IBuffer>(slot->buffer)->setClientData(slot.get());
        check(cam.output->releaseBuffer(slot->buffer.get()) == STATUS_OK,
              "Argus buffer pool initialization failed");
        cam.slots.push_back(std::move(slot));
    }
    cam.request.reset(cam.capture->createRequest(CAPTURE_INTENT_VIDEO_RECORD));
    auto* req = interface_cast<IRequest>(cam.request);
    auto* source = interface_cast<ISourceSettings>(cam.request);
    check(req && source && req->enableOutputStream(cam.stream.get()) == STATUS_OK &&
          source->setSensorMode(chosen) == STATUS_OK &&
          source->setFrameDurationRange(Range<uint64_t>(duration)) == STATUS_OK,
          "Argus capture configuration rejected");
    std::printf("camera=%d mode=%dx%d duration_ns=%llu buffers=%zu\n", index,
                options.width, options.height, static_cast<unsigned long long>(duration), kBuffers);
}
void consume(Camera& cam, Shared& shared, int index) {
    while (!shared.stop) {
        Status status = STATUS_OK;
        Buffer* buffer = cam.output->acquireBuffer(200000000ULL, &status);
        if (!buffer) {
            if (status == STATUS_TIMEOUT) {
                std::lock_guard<std::mutex> lock(shared.mutex);
                ++shared.timeouts[index];
                continue;
            }
            if (!shared.stop) shared.failed = true;
            break;
        }
        auto* wrapped = interface_cast<IBuffer>(buffer);
        auto* meta = wrapped ? interface_cast<const ICaptureMetadata>(wrapped->getMetadata()) : nullptr;
        if (!meta || !meta->getSensorTimestamp()) {
            std::fprintf(stderr, "camera=%d missing sensor-origin timestamp\n", index);
            shared.failed = true;
            cam.output->releaseBuffer(buffer);
            break;
        }
        const Frame frame{buffer, meta->getSensorTimestamp()};
        const uint32_t captureId = meta->getCaptureId();
        const auto* internal = shared.internalAvailable ?
            interface_cast<const Ext::IInternalFrameCount>(wrapped->getMetadata()) : nullptr;
        if (shared.internalAvailable && !internal) {
            std::fprintf(stderr, "camera=%d internal frame-count extension missing from metadata\n", index);
            shared.failed = true;
            cam.output->releaseBuffer(buffer);
            break;
        }
        const uint64_t internalCount = internal ? internal->getInternalFrameCount() : 0;
        bool dropped = false;
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            const auto window = windowFor(Clock::now() - shared.started);
            ++shared.received[index];
            ++shared.windowReceived[window][index];
            if (shared.lastTimestamp[index] && frame.ts <= shared.lastTimestamp[index]) {
                ++shared.reordered[index]; ++shared.windowReordered[window][index];
            }
            if (shared.lastTimestamp[index] && frame.ts > shared.lastTimestamp[index] + kFrameNs * 3 / 2) {
                const auto gap = (frame.ts - shared.lastTimestamp[index] + kFrameNs / 2) / kFrameNs - 1;
                shared.timestampGaps[index] += gap;
                shared.windowTimestampGaps[window][index] += gap;
            }
            if (shared.received[index] > 1 && captureId > shared.lastCaptureId[index] + 1) {
                const auto gap = captureId - shared.lastCaptureId[index] - 1;
                shared.captureGaps[index] += gap;
                shared.windowCaptureGaps[window][index] += gap;
            }
            if (internal && shared.internalBaseline[index] &&
                internalCount > shared.lastInternalCount[index] + 1) {
                const auto gap = internalCount - shared.lastInternalCount[index] - 1;
                shared.internalGaps[index] += gap;
                shared.windowInternalGaps[window][index] += gap;
            }
            if (internal) {
                shared.lastInternalCount[index] = internalCount;
                shared.internalBaseline[index] = true;
            }
            shared.lastTimestamp[index] = frame.ts;
            shared.lastCaptureId[index] = captureId;
            dropped = shared.queued[index].size() >= kQueued;
            if (dropped) { ++shared.drops[index]; ++shared.windowDrops[window][index]; }
            else shared.queued[index].push_back(frame);
        }
        shared.ready.notify_one();
        if (dropped && cam.output->releaseBuffer(buffer) != STATUS_OK) {
            shared.failed = true;
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.exited[index] = true;
    }
    shared.ready.notify_one();
}
void release(const Pair& pair, std::array<Camera, 2>& cameras) {
    for (int i = 0; i < 2; ++i)
        check(cameras[i].output->releaseBuffer(pair[i].buffer) == STATUS_OK, "Argus releaseBuffer failed");
}

// Compiled on the Jetson by NVRTC: avoids a host-side nvcc/toolchain and never maps a source pixel on CPU.
constexpr const char* kKernels = R"CUDA(
__device__ float bilinear(const unsigned char* p, int pitch, int width, int height,
                           float x, float y, int stride, int channel) {
 x = fminf(width - 1.f, fmaxf(0.f, x));
 y = fminf(height - 1.f, fmaxf(0.f, y));
 int ix = (int)x, iy = (int)y, jx = min(ix + 1, width - 1), jy = min(iy + 1, height - 1);
 float a = x - ix, b = y - iy;
 float top = p[iy * pitch + ix * stride + channel] * (1.f-a) + p[iy * pitch + jx * stride + channel] * a;
 float bottom = p[jy * pitch + ix * stride + channel] * (1.f-a) + p[jy * pitch + jx * stride + channel] * a;
 return top * (1.f-b) + bottom * b;
}
)CUDA" E2E_STRINGIFY(E2E_VISIBILITY(__device__)) R"CUDA(
struct Rgb { float x, y, z; };
__device__ Rgb cameraRgb(const unsigned char* yy, const unsigned char* uv, int yp, int up,
                            int sw, int sh, float rayX, float rayY, float rayZ,
                            float yaw, float pitch, float roll) {
 // Placeholder rectilinear intrinsics (76-degree horizontal FOV), Brown-Conrady lens coefficients.
 float dx = rayX * cosf(yaw) - rayZ * sinf(yaw);
 float dz = rayX * sinf(yaw) + rayZ * cosf(yaw);
 float dy = rayY - pitch * dz + roll * dx;
 float nx = dx / dz, ny = dy / dz;
 float r2 = nx*nx + ny*ny;
 float radial = 1.f - .045f*r2 + .009f*r2*r2 - .0008f*r2*r2*r2;
 float xd = nx*radial + 2.f*.0007f*nx*ny + .0004f*(r2+2.f*nx*nx);
 float yd = ny*radial + .0007f*(r2+2.f*ny*ny) + 2.f*.0004f*nx*ny;
 float fx = sw * .5f / tanf(76.f * .00872664626f);
 float sx = sw*.5f + xd*fx, sy = sh*.5f + yd*fx;
 float Y = bilinear(yy, yp, sw, sh, sx, sy, 1, 0);
 float U = bilinear(uv, up, sw/2, sh/2, sx*.5f, sy*.5f, 2, 0) - 128.f;
 float V = bilinear(uv, up, sw/2, sh/2, sx*.5f, sy*.5f, 2, 1) - 128.f;
 return {Y+1.402f*V, Y-.344136f*U-.714136f*V, Y+1.772f*U};
}
__device__ Rgb pixel(const unsigned char* y0, const unsigned char* uv0,
 const unsigned char* y1, const unsigned char* uv1, int py0, int puv0, int py1, int puv1,
 int sw, int sh, float zoom, float pan, int x, int row, const unsigned char* clip) {
 float rx = ((x+.5f-960.f)/960.f + pan) / zoom;
 float ry = (row+.5f-540.f)/960.f/zoom;
 float az = atan2f(rx, 1.f);
 // Placeholder geometry: approximate yaw +/-36 degrees; pitch and roll +/-2 degrees.
 float yaw0 = -36.f*.01745329252f, yaw1 = 36.f*.01745329252f;
 // Only project a contributing camera; at a pan edge the other ray may point behind it.
 Visibility visible = visibility(az, yaw0, yaw1);
 Rgb rgb = {0.f, 0.f, 0.f};
 if (visible.left) {
  Rgb a = cameraRgb(y0, uv0, py0, puv0, sw, sh, rx, ry, 1.f, yaw0, .025f, -.017f);
  rgb.x += (a.x*1.02f+a.y*.005f)*(1.f-visible.weight);
  rgb.y += (a.y*.995f+a.z*.003f)*(1.f-visible.weight);
  rgb.z += (a.z*1.01f+a.x*.002f)*(1.f-visible.weight);
 }
 if (visible.right) {
  Rgb b = cameraRgb(y1, uv1, py1, puv1, sw, sh, rx, ry, 1.f, yaw1, -.025f, .017f);
  rgb.x += (b.x*.99f+b.y*.004f)*visible.weight;
  rgb.y += (b.y*1.01f+b.z*.002f)*visible.weight;
  rgb.z += (b.z*.98f+b.x*.003f)*visible.weight;
 }
 int i = row*1920+x;
 float cy = clip[i], cu = clip[1920*1080 + (row/2)*960+x/2]-128.f;
 float cv = clip[1920*1080*5/4 + (row/2)*960+x/2]-128.f;
 rgb.x = .7f*rgb.x+.3f*(cy+1.402f*cv);
 rgb.y = .7f*rgb.y+.3f*(cy-.344136f*cu-.714136f*cv);
 rgb.z = .7f*rgb.z+.3f*(cy+1.772f*cu);
 return rgb;
}
extern "C" __global__ void render(const unsigned char* y0, const unsigned char* uv0,
 const unsigned char* y1, const unsigned char* uv1, int py0, int puv0, int py1, int puv1,
 int sw, int sh, float zoom, float pan, const unsigned char* clip, unsigned char* out) {
 int x = blockIdx.x * blockDim.x + threadIdx.x, row = blockIdx.y * blockDim.y + threadIdx.y;
 if (x >= 1920 || row >= 1080) return;
 Rgb rgb = pixel(y0,uv0,y1,uv1,py0,puv0,py1,puv1,sw,sh,zoom,pan,x,row,clip);
 out[row*1920+x] = (unsigned char)fminf(255.f,fmaxf(0.f,.299f*rgb.x+.587f*rgb.y+.114f*rgb.z));
 if (!(x&1) && !(row&1)) {
   int offset = 1920*1080 + (row/2)*960+x/2;
   out[offset] = (unsigned char)fminf(255.f,fmaxf(0.f,128.f-.168736f*rgb.x-.331264f*rgb.y+.5f*rgb.z));
   out[offset+960*540] = (unsigned char)fminf(255.f,fmaxf(0.f,128.f+.5f*rgb.x-.418688f*rgb.y-.081312f*rgb.z));
 }
}
extern "C" __global__ void prepare(const unsigned char* y, const unsigned char* uv,
 int py, int puv, int sw, int sh, float* tensor, int w, int h, int batch) {
 int x = blockIdx.x * blockDim.x + threadIdx.x, row = blockIdx.y * blockDim.y + threadIdx.y;
 if (x >= w || row >= h) return;
 int sx = min(sw - 1, (int)((x + .5f) * sw / w));
 int sy = min(sh - 1, (int)((row + .5f) * sh / h));
 float Y = y[sy * py + sx], U = uv[(sy / 2) * puv + (sx & ~1)] - 128.f;
 float V = uv[(sy / 2) * puv + (sx & ~1) + 1] - 128.f;
 int offset = batch * 3 * w * h + row * w + x;
 tensor[offset] = (fminf(255.f, fmaxf(0.f, Y + 1.402f * V)) / 255.f - .485f) / .229f;
 tensor[offset + w * h] = (fminf(255.f, fmaxf(0.f, Y - .344136f * U - .714136f * V)) / 255.f - .456f) / .224f;
 tensor[offset + 2 * w * h] = (fminf(255.f, fmaxf(0.f, Y + 1.772f * U)) / 255.f - .406f) / .225f;
}
)CUDA";
struct Mapped {
    CUgraphicsResource resource = nullptr;
    CUeglFrame frame{};
    int yPitch = 0, uvPitch = 0;
    explicit Mapped(Buffer* buffer) {
        auto* wrapped = interface_cast<IBuffer>(buffer);
        auto* slot = wrapped ? static_cast<const Slot*>(wrapped->getClientData()) : nullptr;
        check(slot != nullptr, "Argus buffer has no surface owner");
        driverCheck(cuGraphicsEGLRegisterImage(&resource, slot->image, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE),
                    "CUDA EGL register");
        try {
            driverCheck(cuGraphicsResourceGetMappedEglFrame(&frame, resource, 0, 0), "CUDA EGL map");
            check(frame.frameType == CU_EGL_FRAME_TYPE_PITCH && frame.planeCount >= 2 &&
                  frame.frame.pPitch[0] && frame.frame.pPitch[1] &&
                  slot->surface->surfaceList[0].planeParams.pitch[0] >= frame.width &&
                  slot->surface->surfaceList[0].planeParams.pitch[1] >= frame.width,
                  "NV12 CUDA pitched EGL interop unavailable");
            yPitch = slot->surface->surfaceList[0].planeParams.pitch[0];
            uvPitch = slot->surface->surfaceList[0].planeParams.pitch[1];
        } catch (...) {
            cuGraphicsUnregisterResource(resource);
            resource = nullptr;
            throw;
        }
    }
    ~Mapped() { if (resource) cuGraphicsUnregisterResource(resource); }
    Mapped(const Mapped&) = delete;
    Mapped& operator=(const Mapped&) = delete;
    const void* y() const { return frame.frame.pPitch[0]; }
    const void* uv() const { return frame.frame.pPitch[1]; }
};
class Gpu {
    CUmodule module_ = nullptr;
    CUfunction render_ = nullptr, prepare_ = nullptr;
    void* output_ = nullptr;
    void* clip_ = nullptr;
    cudaStream_t renderStream_ = nullptr, analysisStream_ = nullptr;
    size_t outputBytes_ = kOutputWidth * kOutputHeight * 3 / 2;
public:
    Gpu() {
        cudaCheck(cudaFree(nullptr), "initialize CUDA runtime");
        nvrtcProgram program = nullptr;
        rtcCheck(nvrtcCreateProgram(&program, kKernels, "virtual-camera.cu", 0, nullptr, nullptr), "NVRTC create");
        const char* flags[] = {"--gpu-architecture=compute_87", "--std=c++14"};
        auto result = nvrtcCompileProgram(program, 2, flags);
        if (result != NVRTC_SUCCESS) {
            size_t size = 0;
            nvrtcGetProgramLogSize(program, &size);
            std::string log(size, '\0');
            nvrtcGetProgramLog(program, log.data());
            nvrtcDestroyProgram(&program);
            throw std::runtime_error("NVRTC compile: " + log);
        }
        size_t size = 0;
        rtcCheck(nvrtcGetPTXSize(program, &size), "NVRTC PTX size");
        std::string ptx(size, '\0');
        rtcCheck(nvrtcGetPTX(program, ptx.data()), "NVRTC PTX");
        rtcCheck(nvrtcDestroyProgram(&program), "NVRTC destroy");
        driverCheck(cuModuleLoadData(&module_, ptx.data()), "load CUDA kernels");
        driverCheck(cuModuleGetFunction(&render_, module_, "render"), "find render kernel");
        driverCheck(cuModuleGetFunction(&prepare_, module_, "prepare"), "find prepare kernel");
        cudaCheck(cudaMalloc(&output_, outputBytes_), "allocate 1080p I420 output");
        cudaCheck(cudaMalloc(&clip_, outputBytes_), "allocate decoded clip frame");
        cudaCheck(cudaStreamCreateWithFlags(&renderStream_, cudaStreamNonBlocking), "render stream");
        cudaCheck(cudaStreamCreateWithFlags(&analysisStream_, cudaStreamNonBlocking), "analysis stream");
    }
    cudaStream_t analysisStream() const { return analysisStream_; }
    ~Gpu() {
        if (renderStream_) cudaStreamDestroy(renderStream_);
        if (analysisStream_) cudaStreamDestroy(analysisStream_);
        if (output_) cudaFree(output_);
        if (clip_) cudaFree(clip_);
        if (module_) cuModuleUnload(module_);
    }
    double render(const Pair& pair, int sw, int sh, float pan, float zoom,
                  const unsigned char* clip, unsigned char* destination) {
        const auto start = Clock::now();
        Mapped left(pair[0].buffer), right(pair[1].buffer);
        auto y0 = left.y(), uv0 = left.uv(), y1 = right.y(), uv1 = right.uv();
        int py0 = left.yPitch, puv0 = left.uvPitch, py1 = right.yPitch, puv1 = right.uvPitch;
        cudaCheck(cudaMemcpy(clip_, clip, outputBytes_, cudaMemcpyHostToDevice), "upload decoded clip");
        void* args[] = {&y0, &uv0, &y1, &uv1, &py0, &puv0, &py1, &puv1,
                        &sw, &sh, &zoom, &pan, &clip_, &output_};
        driverCheck(cuLaunchKernel(render_, 120, 68, 1, 16, 16, 1, 0,
                                   reinterpret_cast<CUstream>(renderStream_), args, nullptr), "render launch");
        cudaCheck(cudaStreamSynchronize(renderStream_), "render synchronization");
        cudaCheck(cudaMemcpy(destination, output_, outputBytes_, cudaMemcpyDeviceToHost),
                  "copy 1080p I420 to encoder");
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    double prepare(const Pair& pair, int sw, int sh, void* tensor, int w, int h) {
        const auto start = Clock::now();
        Mapped left(pair[0].buffer), right(pair[1].buffer);
        for (int batch = 0; batch < 2; ++batch) {
            auto& src = batch == 0 ? left : right;
            auto y = src.y(), uv = src.uv();
            int py = src.yPitch, puv = src.uvPitch;
            void* args[] = {&y, &uv, &py, &puv, &sw, &sh, &tensor, &w, &h, &batch};
            driverCheck(cuLaunchKernel(prepare_, (w + 15) / 16, (h + 15) / 16, 1,
                                       16, 16, 1, 0, reinterpret_cast<CUstream>(analysisStream_),
                                       args, nullptr), "tensor prep launch");
        }
        cudaCheck(cudaStreamSynchronize(analysisStream_), "tensor prep synchronization");
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
};

struct TrtLogger : nvinfer1::ILogger {
    void log(Severity severity, const char* message) noexcept override {
        if (severity <= Severity::kWARNING) std::fprintf(stderr, "TensorRT: %s\n", message);
    }
};
template <typename T> struct TrtDeleter { void operator()(T* p) const { delete p; } };
struct DetectionBox { int camera; float cx, cy, w, h, score; };
class Detector {
    TrtLogger logger_;
    std::unique_ptr<nvinfer1::IRuntime, TrtDeleter<nvinfer1::IRuntime>> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine, TrtDeleter<nvinfer1::ICudaEngine>> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext, TrtDeleter<nvinfer1::IExecutionContext>> context_;
    std::vector<void*> allocations_;
    struct Output { std::string name; void* device; size_t size; };
    std::vector<Output> outputs_;
    void* input_ = nullptr;
    int width_ = 0, height_ = 0;
    std::vector<DetectionBox> boxes_;
public:
    explicit Detector(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        check(file.good(), "TensorRT engine file unavailable: " + path);
        const std::vector<char> bytes(std::istreambuf_iterator<char>{file}, {});
        check(!bytes.empty(), "TensorRT engine file empty");
        runtime_.reset(nvinfer1::createInferRuntime(logger_));
        check(runtime_ != nullptr, "TensorRT runtime unavailable");
        engine_.reset(runtime_->deserializeCudaEngine(bytes.data(), bytes.size()));
        check(engine_ != nullptr, "TensorRT engine incompatible/deserialize failed");
        context_.reset(engine_->createExecutionContext());
        check(context_ != nullptr, "TensorRT execution context unavailable");
        int inputs = 0, outputs = 0;
        for (int i = 0; i < engine_->getNbIOTensors(); ++i) {
            const char* name = engine_->getIOTensorName(i);
            auto dims = engine_->getTensorShape(name);
            auto mode = engine_->getTensorIOMode(name);
            check(engine_->getTensorDataType(name) == nvinfer1::DataType::kFLOAT,
                  std::string("requires FP32 tensor: ") + name);
            if (mode == nvinfer1::TensorIOMode::kINPUT) {
                ++inputs;
                check(dims.nbDims == 4 && (dims.d[0] == 2 || dims.d[0] == -1) &&
                      dims.d[1] == 3 && dims.d[2] == 512 && dims.d[3] == 512,
                      "RF-DETR input must be FP32 NCHW [2,3,512,512]");
                dims.d[0] = 2;
                check(context_->setInputShape(name, dims), "TensorRT batch=2 shape rejected");
                width_ = dims.d[3]; height_ = dims.d[2];
            } else ++outputs;
        }
        check(inputs == 1 && outputs == 2, "engine must expose one input and two outputs");
        for (int i = 0; i < engine_->getNbIOTensors(); ++i) {
            const char* name = engine_->getIOTensorName(i);
            const auto dims = context_->getTensorShape(name);
            size_t elements = 1;
            for (int d = 0; d < dims.nbDims; ++d) {
                check(dims.d[d] > 0 && dims.d[d] <= 1000000, std::string("unresolved tensor dimension: ") + name);
                elements *= static_cast<size_t>(dims.d[d]);
                check(elements <= 2000000, std::string("tensor too large: ") + name);
            }
            const size_t size = elements * sizeof(float);
            void* device = nullptr;
            cudaCheck(cudaMalloc(&device, size), "allocate TensorRT IO");
            allocations_.push_back(device);
            check(context_->setTensorAddress(name, device), std::string("bind TensorRT IO: ") + name);
            if (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) input_ = device;
            else outputs_.push_back({name, device, size});
            std::printf("trt_tensor=%s elements=%zu role=%s\n", name, elements,
                        engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT ? "input" : "output");
        }
        check(outputs_.size() == 2 &&
              std::any_of(outputs_.begin(), outputs_.end(), [](const Output& o) {
                  return o.name == "dets" && o.size == 2 * 300 * 4 * sizeof(float);
              }) &&
              std::any_of(outputs_.begin(), outputs_.end(), [](const Output& o) {
                  return o.name == "labels" && o.size == 2 * 300 * 91 * sizeof(float);
              }), "engine must expose RF-DETR Small dets [2,300,4] and labels [2,300,91]");
    }
    ~Detector() {
        context_.reset();
        for (auto* p : allocations_) cudaFree(p);
    }
    int width() const { return width_; }
    int height() const { return height_; }
    double infer(cudaStream_t stream) {
        const auto start = Clock::now();
        check(context_->enqueueV3(stream), "TensorRT enqueue failed");
        cudaCheck(cudaStreamSynchronize(stream), "TensorRT inference failed");
        std::array<std::vector<float>, 2> host;
        for (size_t i = 0; i < outputs_.size(); ++i) {
            const auto& output = outputs_[i];
            host[i].resize(output.size / sizeof(float));
            cudaCheck(cudaMemcpy(host[i].data(), output.device, output.size, cudaMemcpyDeviceToHost),
                      "TensorRT output readback");
            check(std::all_of(host[i].begin(), host[i].end(), [](float v) { return std::isfinite(v); }),
                  "TensorRT output contains non-finite values");
        }
        const auto& dets = outputs_[0].name == "dets" ? host[0] : host[1];
        const auto& labels = outputs_[0].name == "labels" ? host[0] : host[1];
        std::vector<DetectionBox> current;
        for (int camera = 0; camera < 2; ++camera) {
            for (int box = 0; box < 300; ++box) {
                const float* logits = labels.data() + (camera * 300 + box) * 91;
                const float personScore = 1.f / (1.f + std::exp(-logits[0]));
                const float* det = dets.data() + (camera * 300 + box) * 4;
                if (personScore >= .5f && det[0] >= 0 && det[0] <= 1 && det[1] >= 0 && det[1] <= 1 &&
                    det[2] > 0 && det[2] <= 1 && det[3] > 0 && det[3] <= 1)
                    current.push_back({camera, det[0], det[1], det[2], det[3], personScore});
            }
        }
        boxes_ = std::move(current);
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    void* input() const { return input_; }
    const std::vector<DetectionBox>& boxes() const { return boxes_; }
};

class Clip {
    GstElement* pipeline_ = nullptr;
    GstElement* sink_ = nullptr;
    GstBus* bus_ = nullptr;
    bool hardware_ = false;
    std::vector<unsigned char> frame_;
    uint64_t frames_ = 0;
    double decodeMs_ = 0;
    void close() {
        if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (bus_) gst_object_unref(bus_);
        if (sink_) gst_object_unref(sink_);
        if (pipeline_) gst_object_unref(pipeline_);
        pipeline_ = nullptr; sink_ = nullptr; bus_ = nullptr;
    }
    bool open(const std::string& path, bool hardware) {
        close();
        hardware_ = hardware;
        const char* chain = hardware ?
            "matroskademux ! vp9parse ! nvv4l2decoder ! nvvidconv ! video/x-raw,format=I420,width=1920,height=1080 ! " :
            "decodebin ! videoconvert ! videoscale ! video/x-raw,format=I420,width=1920,height=1080 ! ";
        const std::string pipeline = "filesrc name=clipfile ! " + std::string(chain) +
            "appsink name=clipsink sync=false max-buffers=2 drop=false";
        GError* error = nullptr;
        pipeline_ = gst_parse_launch(pipeline.c_str(), &error);
        if (error) { g_error_free(error); close(); return false; }
        if (!pipeline_) return false;
        auto* source = gst_bin_get_by_name(GST_BIN(pipeline_), "clipfile");
        sink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "clipsink");
        if (!source || !sink_) {
            if (source) gst_object_unref(source);
            close(); return false;
        }
        g_object_set(source, "location", path.c_str(), nullptr);
        gst_object_unref(source);
        bus_ = gst_element_get_bus(pipeline_);
        if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            close(); return false;
        }
        return true;
    }
public:
    explicit Clip(const std::string& path) {
        check(open(path, true) || open(path, false), "neither VP9 hardware nor software decoder available");
        try { next(); }
        catch (...) {
            if (!hardware_ || !open(path, false)) throw;
            next();
        }
        std::printf("clip_decoder=%s clip_blend=CUDA clip_source=%s\n",
                    hardware_ ? "nvv4l2decoder" : "software_decodebin", path.c_str());
    }
    ~Clip() { close(); }
    const unsigned char* data() const { return frame_.data(); }
    uint64_t frames() const { return frames_; }
    double decodeMs() const { return decodeMs_; }
    void next() {
        const auto start = Clock::now();
        GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink_), 5 * GST_SECOND);
        if (!sample && gst_app_sink_is_eos(GST_APP_SINK(sink_))) {
            check(gst_element_seek_simple(pipeline_, GST_FORMAT_TIME,
                  static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0),
                  "clip seek to beginning failed");
            sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink_), 5 * GST_SECOND);
        }
        if (!sample) {
            GstMessage* error = gst_bus_pop_filtered(bus_, GST_MESSAGE_ERROR);
            if (error) {
                GError* detail = nullptr; gchar* debug = nullptr;
                gst_message_parse_error(error, &detail, &debug);
                const std::string text = detail ? detail->message : "unknown decoder failure";
                if (detail) g_error_free(detail);
                g_free(debug); gst_message_unref(error);
                throw std::runtime_error("clip decoder: " + text);
            }
            throw std::runtime_error("clip decoder timed out or returned no frame");
        }
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstVideoInfo info{};
        auto* caps = gst_sample_get_caps(sample);
        if (!buffer || !caps || !gst_video_info_from_caps(&info, caps) ||
            GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_I420 ||
            GST_VIDEO_INFO_WIDTH(&info) != kOutputWidth ||
            GST_VIDEO_INFO_HEIGHT(&info) != kOutputHeight) {
            gst_sample_unref(sample);
            throw std::runtime_error("clip decoder did not produce 1920x1080 I420");
        }
        if (auto* meta = gst_buffer_get_video_meta(buffer)) {
            if (meta->format != GST_VIDEO_FORMAT_I420 || meta->width != kOutputWidth ||
                meta->height != kOutputHeight) {
                gst_sample_unref(sample);
                throw std::runtime_error("clip video metadata differs from caps");
            }
        }
        GstVideoFrame video{};
        if (!gst_video_frame_map(&video, &info, buffer, GST_MAP_READ)) {
            gst_sample_unref(sample);
            throw std::runtime_error("map decoded clip planes failed");
        }
        constexpr size_t size = kOutputWidth * kOutputHeight * 3 / 2;
        frame_.resize(size);
        const int widths[] = {kOutputWidth, kOutputWidth / 2, kOutputWidth / 2};
        const int heights[] = {kOutputHeight, kOutputHeight / 2, kOutputHeight / 2};
        size_t offset = 0;
        for (int plane = 0; plane < 3; ++plane) {
            const auto* src = static_cast<const unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&video, plane));
            const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&video, plane);
            if (!src || stride < widths[plane]) {
                gst_video_frame_unmap(&video); gst_sample_unref(sample);
                throw std::runtime_error("clip decoder plane stride invalid");
            }
            for (int row = 0; row < heights[plane]; ++row)
                std::copy_n(src + row * stride, widths[plane], frame_.data() + offset + row * widths[plane]);
            offset += widths[plane] * heights[plane];
        }
        gst_video_frame_unmap(&video); gst_sample_unref(sample);
        ++frames_;
        decodeMs_ += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
};
struct GstCounts { std::atomic<uint64_t> mp4In{0}, mp4Out{0}, rtmpIn{0}, rtmpOut{0}; };
GstPadProbeReturn countBuffer(GstPad*, GstPadProbeInfo* info, gpointer data) {
    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)
        ++(*static_cast<std::atomic<uint64_t>*>(data));
    return GST_PAD_PROBE_OK;
}
class Encoder {
    GstElement* pipeline_ = nullptr;
    GstElement* clean_ = nullptr;
    GstElement* overlay_ = nullptr;
    GstBus* bus_ = nullptr;
    GstCounts counts_;
    static void probe(GstElement* pipeline, const char* name, const char* pad,
                      std::atomic<uint64_t>& target) {
        GstElement* element = gst_bin_get_by_name(GST_BIN(pipeline), name);
        check(element != nullptr, std::string("GStreamer element missing: ") + name);
        GstPad* resolved = gst_element_get_static_pad(element, pad);
        check(resolved != nullptr, std::string("GStreamer pad missing: ") + name);
        gst_pad_add_probe(resolved, GST_PAD_PROBE_TYPE_BUFFER, countBuffer, &target, nullptr);
        gst_object_unref(resolved);
        gst_object_unref(element);
    }
public:
    Encoder(const Options& options, int* argc, char*** argv) {
        gst_init(argc, argv);
        GError* error = nullptr;
        const std::string raw = " format=time is-live=true block=false caps=video/x-raw,format=I420,width=1920,height=1080,framerate=30/1 ";
        const std::string preset = " ! queue max-size-buffers=3 max-size-bytes=0 max-size-time=0 leaky=downstream ! x264enc speed-preset=" + options.preset + " tune=zerolatency key-int-max=30 bitrate=8000 ";
        const std::string launch = "appsrc name=clean" + raw + preset +
            "name=mp4encoder ! h264parse name=mp4parse ! mp4mux ! filesink name=record "
            "appsrc name=overlay" + raw + preset +
            "name=rtmpencoder ! h264parse name=netparse ! flvmux streamable=true ! rtmpsink name=network sync=false";
        pipeline_ = gst_parse_launch(launch.c_str(), &error);
        if (error) {
            std::string message(error->message);
            g_error_free(error);
            throw std::runtime_error("GStreamer parse: " + message);
        }
        check(pipeline_ != nullptr, "GStreamer pipeline unavailable (x264enc/rtmpsink required)");
        clean_ = gst_bin_get_by_name(GST_BIN(pipeline_), "clean");
        overlay_ = gst_bin_get_by_name(GST_BIN(pipeline_), "overlay");
        GstElement* record = gst_bin_get_by_name(GST_BIN(pipeline_), "record");
        GstElement* network = gst_bin_get_by_name(GST_BIN(pipeline_), "network");
        check(clean_ && overlay_ && record && network, "GStreamer sink lookup failed");
        g_object_set(record, "location", options.mp4.c_str(), nullptr);
        g_object_set(network, "location", options.rtmp.c_str(), nullptr);
        gst_object_unref(record);
        gst_object_unref(network);
        probe(pipeline_, "mp4encoder", "sink", counts_.mp4In);
        probe(pipeline_, "mp4encoder", "src", counts_.mp4Out);
        probe(pipeline_, "rtmpencoder", "sink", counts_.rtmpIn);
        probe(pipeline_, "rtmpencoder", "src", counts_.rtmpOut);
        std::printf("encoders=independent preset=%s bitrate_kbps=8000,8000 overlay=raw_rfdetr_person_boxes_approximate_coordinates\n", options.preset.c_str());
        bus_ = gst_element_get_bus(pipeline_);
        check(gst_element_set_state(pipeline_, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
              "GStreamer encoder startup failed");
    }
    ~Encoder() {
        if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (bus_) gst_object_unref(bus_);
        if (clean_) gst_object_unref(clean_);
        if (overlay_) gst_object_unref(overlay_);
        if (pipeline_) gst_object_unref(pipeline_);
    }
    std::array<uint64_t, 4> counts() const {
        return {counts_.mp4In.load(), counts_.mp4Out.load(),
                counts_.rtmpIn.load(), counts_.rtmpOut.load()};
    }
    void errors() {
        GstMessage* message = gst_bus_pop_filtered(bus_, GST_MESSAGE_ERROR);
        if (!message) return;
        GError* error = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        const std::string text = error ? error->message : "unknown GStreamer error";
        if (error) g_error_free(error);
        g_free(debug);
        gst_message_unref(message);
        throw std::runtime_error("GStreamer: " + text);
    }
    void push(const Pair& pair, uint64_t first, Gpu& gpu, Clip& clip, int sw, int sh,
              float pan, float zoom, const std::vector<DetectionBox>& boxes, double& renderMs) {
        constexpr size_t size = kOutputWidth * kOutputHeight * 3 / 2;
        GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
        check(buffer != nullptr, "allocate encoder frame failed");
        GstMapInfo map{};
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            gst_buffer_unref(buffer);
            throw std::runtime_error("map 1080p encoder frame failed");
        }
        try { renderMs = gpu.render(pair, sw, sh, pan, zoom, clip.data(), map.data); }
        catch (...) { gst_buffer_unmap(buffer, &map); gst_buffer_unref(buffer); throw; }
        gst_buffer_unmap(buffer, &map);
        const GstClockTime pts = presentationNs(pair[0].ts, first);
        GST_BUFFER_PTS(buffer) = pts;
        GST_BUFFER_DURATION(buffer) = kFrameNs;
        GstBuffer* overlay = gst_buffer_copy_deep(buffer);
        if (!overlay) {
            gst_buffer_unref(buffer);
            throw std::runtime_error("allocate overlay frame failed");
        }
        if (!gst_buffer_map(overlay, &map, GST_MAP_WRITE)) {
            gst_buffer_unref(overlay); gst_buffer_unref(buffer);
            throw std::runtime_error("map overlay frame failed");
        }
        // RF-DETR boxes are real tensor output; mapping via placeholder geometry is uncalibrated.
        for (const auto& box : boxes) {
            const float yaw = box.camera ? 36.f : -36.f;
            const auto mapX = [&](float u) {
                const float angle = std::atan((u-.5f)*2.f*std::tan(38.f*.01745329252f)) + yaw*.01745329252f;
                const float virtualX = (std::tan(angle)*zoom - pan)*960.f + 960.f;
                return std::clamp(static_cast<int>(virtualX), 0, kOutputWidth-1);
            };
            const int bx = mapX(box.cx - box.w*.5f);
            const int ex = mapX(box.cx + box.w*.5f);
            const int by = std::clamp(static_cast<int>((box.cy-box.h*.5f)*kOutputHeight), 0, kOutputHeight-1);
            const int ey = std::clamp(static_cast<int>((box.cy+box.h*.5f)*kOutputHeight), 0, kOutputHeight-1);
            for (int y = by; y <= ey; ++y)
                for (int x = bx; x <= ex; ++x)
                    if (y < by+3 || y > ey-3 || x < bx+3 || x > ex-3)
                        map.data[y*kOutputWidth+x] = 235;
        }
        gst_buffer_unmap(overlay, &map);
        GST_BUFFER_PTS(overlay) = pts;
        GST_BUFFER_DURATION(overlay) = kFrameNs;
        const auto cleanResult = gst_app_src_push_buffer(GST_APP_SRC(clean_), buffer);
        const auto overlayResult = gst_app_src_push_buffer(GST_APP_SRC(overlay_), overlay);
        check(cleanResult == GST_FLOW_OK && overlayResult == GST_FLOW_OK, "encoder input rejected frame");
        errors();
    }
    void finish() {
        const auto cleanResult = gst_app_src_end_of_stream(GST_APP_SRC(clean_));
        const auto overlayResult = gst_app_src_end_of_stream(GST_APP_SRC(overlay_));
        check(cleanResult == GST_FLOW_OK && overlayResult == GST_FLOW_OK,
              "encoder EOS request failed");
        GstMessage* message = gst_bus_timed_pop_filtered(bus_, 300 * GST_SECOND,
                                             static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        check(message != nullptr, "MP4 finalization timed out without EOS");
        const bool eos = GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
        if (!eos) {
            GError* error = nullptr; gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            std::string text = error ? error->message : "unknown error";
            if (error) g_error_free(error);
            g_free(debug);
            gst_message_unref(message);
            throw std::runtime_error("MP4 finalization: " + text);
        }
        gst_message_unref(message);
        std::printf("mp4_encoder_in=%llu mp4_encoded=%llu rtmp_encoder_in=%llu rtmp_encoded=%llu mp4_eos=ok\n",
                    static_cast<unsigned long long>(counts_.mp4In.load()),
                    static_cast<unsigned long long>(counts_.mp4Out.load()),
                    static_cast<unsigned long long>(counts_.rtmpIn.load()),
                    static_cast<unsigned long long>(counts_.rtmpOut.load()));
    }
};

int run(int argc, char** argv) {
    const Options options = parse(argc, argv);
    // Engine is loaded before Argus to fail explicitly on missing/invalid RF-DETR artifacts.
    Detector detector(options.engine);
    Gpu gpu;
    UniqueObj<CameraProvider> provider(CameraProvider::create());
    auto* api = interface_cast<ICameraProvider>(provider);
    check(api != nullptr, "Argus provider unavailable");
    std::vector<CameraDevice*> devices;
    check(api->getCameraDevices(&devices) == STATUS_OK && devices.size() >= 2,
          "both Argus cameras required; no synthetic substitution");
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    check(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr) == EGL_TRUE,
          "EGL initialization failed");
    struct EGLGuard { EGLDisplay display; ~EGLGuard() { eglTerminate(display); } } displayGuard{display};
    std::array<Camera, 2> cameras;
    for (int i = 0; i < 2; ++i) makeCamera(cameras[i], api, devices[i], display, options, i);
    Encoder encoder(options, &argc, &argv);
    Clip clip(options.clip);
    Shared shared;
    std::array<std::thread, 2> consumers;
    std::deque<std::shared_ptr<Pair>> ring;
    bool filled = false;
    uint64_t paired = 0, rendered = 0, renderMisses = 0, unmatched[2]{}, firstPts = 0;
    double renderMs = 0, minuteRenderMs = 0;
    uint64_t minuteRenderCount = 0;
    std::mutex analysisMutex;
    std::condition_variable analysisReady;
    std::shared_ptr<Pair> analysisLatest;
    Clock::time_point analysisSampleTime;
    bool analysisStop = false;
    std::thread analysisWorker;
    std::atomic<uint64_t> inferred{0}, analysisSkipped{0};
    std::atomic<double> prepMs{0}, inferMs{0};
    std::array<std::vector<double>, 2> inferenceLatencies;
    std::mutex latencyMutex;
    std::mutex boxesMutex;
    std::vector<DetectionBox> latestBoxes;
    std::array<std::atomic<uint64_t>, 2> windowInferred{};
    std::array<uint64_t, 2> windowPairs{}, windowRendered{}, windowMisses{}, windowUnmatched{}, windowAnalysisSkipped{};
    std::array<uint64_t, 4> reportCounts{}, minuteEncoded{};
    uint64_t minuteEncodePending = 0, minuteEncodedPending = 0;
    std::array<uint64_t, 2> minuteCaptured{}, minuteDrops{}, minuteGaps{};
    uint64_t minutePairs = 0, minuteRendered = 0, minuteClipFrames = 0;
    uint64_t minuteInferred = 0, minuteSkipped = 0, minuteUnmatched = 0;
    uint64_t minuteMisses = 0;
    bool minuteBaselineSet = false;
    size_t minuteLatencyStart = 0;
    std::vector<double> minuteLatencies;
    double minuteClipMs = 0;
    auto minuteAt = Clock::now();
    unsigned minute = 0;
    bool allMinutesPassed = true;
    uint64_t steadyEncodedBaseline = 0;
    bool steadyBaselineSet = false;
    Clock::time_point reportAt;
    uint64_t maxRing = 0;
    std::exception_ptr analysisFailure;
    std::exception_ptr failure;
    const auto started = Clock::now(), deadline = started + std::chrono::seconds(options.seconds);
    auto nextReport = started + std::chrono::seconds(1);
    auto nextMinute = started + std::chrono::seconds(kStartupSeconds);
    auto nextAnalysis = started;
    minuteAt = started;
    shared.started = started;
    shared.internalAvailable = api->supportsExtension(EXT_INTERNAL_FRAME_COUNT);
    std::printf("internal_frame_count_extension=%s\n", shared.internalAvailable ? "yes" : "no");
    std::printf("virtual_geometry=per_pixel_rectilinear_yaw_72deg_overlap_180px calibration=PLACEHOLDER_NOT_VALIDATED duration_s=%d ", options.seconds);
    std::printf("engine=%s mode=%dx%d output=1920x1080@30\n", options.engine.c_str(), options.width, options.height);
    reportCounts = encoder.counts();
    reportAt = Clock::now();
    try {
        analysisWorker = std::thread([&] {
            try {
                cudaCheck(cudaFree(nullptr), "initialize CUDA on analysis thread");
                while (true) {
                    std::shared_ptr<Pair> sample;
                    Clock::time_point captured;
                    {
                        std::unique_lock<std::mutex> lock(analysisMutex);
                        analysisReady.wait(lock, [&] { return analysisStop || analysisLatest; });
                        if (analysisStop) break;
                        sample = std::move(analysisLatest);
                        captured = analysisSampleTime;
                    }
                    prepMs = gpu.prepare(*sample, options.width, options.height,
                                         detector.input(), detector.width(), detector.height());
                    inferMs = detector.infer(gpu.analysisStream());
                    {
                        std::lock_guard<std::mutex> lock(boxesMutex);
                        latestBoxes = detector.boxes();
                    }
                    const auto completed = Clock::now();
                    const auto window = windowFor(completed - started);
                    {
                        std::lock_guard<std::mutex> lock(latencyMutex);
                        const double latency = std::chrono::duration<double, std::milli>(completed - captured).count();
                        inferenceLatencies[window].push_back(latency);
                        minuteLatencies.push_back(latency);
                    }
                    ++windowInferred[window];
                    ++inferred;
                }
            } catch (...) {
                analysisFailure = std::current_exception();
                shared.failed = true;
                shared.ready.notify_one();
            }
        });
        for (int i = 0; i < 2; ++i) consumers[i] = std::thread(consume, std::ref(cameras[i]), std::ref(shared), i);
        for (auto& cam : cameras) check(cam.capture->repeat(cam.request.get()) == STATUS_OK, "Argus repeat failed");
        while (Clock::now() < deadline && !shared.failed) {
            Pair pair{};
            int action = -1;
            {
                std::unique_lock<std::mutex> lock(shared.mutex);
                if (shared.queued[0].empty() || shared.queued[1].empty())
                    shared.ready.wait_until(lock, Clock::now() + std::chrono::milliseconds(33), [&] {
                        return shared.failed || shared.exited[0] || shared.exited[1] ||
                               (!shared.queued[0].empty() && !shared.queued[1].empty());
                    });
                check(!shared.exited[0] && !shared.exited[1] && !shared.failed, "capture worker exited");
                action = pairAction(shared.queued[0], shared.queued[1]);
                if (action == 2) {
                    for (int i = 0; i < 2; ++i) {
                        pair[i] = shared.queued[i].front();
                        shared.queued[i].pop_front();
                    }
                } else if (action >= 0) {
                    pair[action] = shared.queued[action].front();
                    shared.queued[action].pop_front();
                    ++unmatched[action];
                    ++windowUnmatched[windowFor(Clock::now() - started)];
                }
            }
            if (action == 2) {
                ++paired;
                const auto skew = distanceNs(pair[0].ts, pair[1].ts);
                std::printf("pair=%llu sensor_ns=%llu,%llu skew_us=%.3f\n",
                            static_cast<unsigned long long>(paired),
                            static_cast<unsigned long long>(pair[0].ts),
                            static_cast<unsigned long long>(pair[1].ts), skew / 1000.0);
                auto held = std::shared_ptr<Pair>(new Pair(pair), [&cameras, &shared](Pair* value) {
                    for (int i = 0; i < 2; ++i)
                        if (cameras[i].output->releaseBuffer((*value)[i].buffer) != STATUS_OK)
                            shared.failed = true;
                    delete value;
                });
                ring.push_back(held);
                ++windowPairs[windowFor(Clock::now() - started)];
                maxRing = std::max<uint64_t>(maxRing, ring.size());
                if (ring.size() >= kRingPairs) filled = true;
                if (Clock::now() >= nextAnalysis) {
                    nextAnalysis += std::chrono::nanoseconds(2 * kFrameNs);
                    if (nextAnalysis <= Clock::now())
                        nextAnalysis = Clock::now() + std::chrono::nanoseconds(2 * kFrameNs);
                    {
                        std::lock_guard<std::mutex> lock(analysisMutex);
                        if (analysisLatest) {
                            ++analysisSkipped;
                            ++windowAnalysisSkipped[windowFor(Clock::now() - started)];
                        }
                        analysisLatest = held;
                        analysisSampleTime = Clock::now();
                    }
                    analysisReady.notify_one();
                }
                // Render is paced by sensor pairs: a free-running 30.000 Hz timer drifted against
                // the 30.009 fps 1080 sensor and overflowed the ring (session 1 and 2 misses).
                if (filled) {
                    auto frame = ring.front();
                    ring.pop_front();
                    if (!firstPts) firstPts = (*frame)[0].ts;
                    std::vector<DetectionBox> boxes;
                    {
                        std::lock_guard<std::mutex> lock(boxesMutex);
                        boxes = latestBoxes;
                    }
                    encoder.push(*frame, firstPts, gpu, clip, options.width, options.height,
                                 panForTick(rendered) - .5f, zoomForTick(rendered), boxes, renderMs);
                    minuteRenderMs += renderMs;
                    ++minuteRenderCount;
                    clip.next();
                    ++rendered;
                    ++windowRendered[windowFor(Clock::now() - started)];
                }
            } else if (action >= 0) {
                check(cameras[action].output->releaseBuffer(pair[action].buffer) == STATUS_OK,
                      "unmatched frame release failed");
            }
            if (!filled && Clock::now() >= started + std::chrono::seconds(3))
                throw std::runtime_error("45-pair delay ring did not warm within three seconds");
            if (!minuteBaselineSet && Clock::now() >= nextMinute) {
                const auto now = Clock::now();
                std::lock_guard<std::mutex> captureLock(shared.mutex);
                std::lock_guard<std::mutex> latencyLock(latencyMutex);
                minuteCaptured = shared.received;
                minuteDrops = shared.drops;
                for (int i = 0; i < 2; ++i)
                    minuteGaps[i] = shared.captureGaps[i] + shared.timestampGaps[i] +
                                    shared.internalGaps[i] + shared.reordered[i];
                minutePairs = paired; minuteRendered = rendered;
                minuteInferred = inferred.load(); minuteSkipped = analysisSkipped.load();
                minuteUnmatched = unmatched[0] + unmatched[1]; minuteMisses = renderMisses;
                minuteClipFrames = clip.frames(); minuteClipMs = clip.decodeMs();
                minuteLatencyStart = minuteLatencies.size();
                minuteEncoded = encoder.counts();
                minuteEncodePending = rendered - std::min(rendered, minuteEncoded[1]);
                minuteEncodedPending = rendered - std::min(rendered, minuteEncoded[3]);
                minuteAt = now;
                minuteBaselineSet = true;
                std::printf("startup_elapsed_s=%.3f ring=%zu rendered=%llu\n",
                            std::chrono::duration<double>(now-started).count(), ring.size(),
                            static_cast<unsigned long long>(rendered));
                nextMinute = started + std::chrono::seconds(kStartupSeconds + 60);
            }
            if (minuteBaselineSet && Clock::now() >= nextMinute) {
                const auto now = Clock::now();
                const double dt = std::chrono::duration<double>(now - minuteAt).count();
                const auto counts = encoder.counts();
                encoder.errors();
                std::array<uint64_t, 2> captured{}, drops{}, gaps{};
                size_t queue = 0;
                {
                    std::lock_guard<std::mutex> lock(shared.mutex);
                    for (int i = 0; i < 2; ++i) {
                        captured[i] = shared.received[i] - minuteCaptured[i];
                        minuteCaptured[i] = shared.received[i];
                        drops[i] = shared.drops[i] - minuteDrops[i];
                        minuteDrops[i] = shared.drops[i];
                        const auto total = shared.captureGaps[i] + shared.timestampGaps[i] +
                                           shared.internalGaps[i] + shared.reordered[i];
                        gaps[i] = total - minuteGaps[i];
                        minuteGaps[i] = total;
                        queue += shared.queued[i].size();
                    }
                }
                const uint64_t pairs = paired - minutePairs, produced = rendered - minuteRendered;
                minutePairs = paired; minuteRendered = rendered;
                const uint64_t batches = inferred.load() - minuteInferred;
                minuteInferred = inferred.load();
                const uint64_t skipped = analysisSkipped.load() - minuteSkipped;
                minuteSkipped = analysisSkipped.load();
                const double possible = std::min(captured[0], captured[1]);
                const double coverage = possible ? 100. * pairs / possible : 0.;
                std::vector<double> latencies;
                {
                    std::lock_guard<std::mutex> lock(latencyMutex);
                    const auto& source = minuteLatencies;
                    if (minuteLatencyStart <= source.size())
                        latencies.assign(source.begin() + minuteLatencyStart, source.end());
                    minuteLatencyStart = source.size();
                }
                const uint64_t unpaired = unmatched[0] + unmatched[1] - minuteUnmatched;
                minuteUnmatched = unmatched[0] + unmatched[1];
                const uint64_t missed = renderMisses - minuteMisses;
                minuteMisses = renderMisses;
                const double mp4 = (counts[1] - minuteEncoded[1]) / dt;
                const double rtmp = (counts[3] - minuteEncoded[3]) / dt;
                const uint64_t mp4Pending = rendered - std::min(rendered, counts[1]);
                const uint64_t rtmpPending = rendered - std::min(rendered, counts[3]);
                const bool noDrops = !(drops[0] || drops[1] || gaps[0] || gaps[1] ||
                                       skipped || unpaired || missed) &&
                                     pairs == produced && coverage >= 99.5 &&
                                     mp4Pending <= 3 && rtmpPending <= 3 &&
                                     mp4Pending <= minuteEncodePending + 3 &&
                                     rtmpPending <= minuteEncodedPending + 3 &&
                                     steadyAcceptance(true, captured[0] / dt,
                                         captured[1] / dt, inferenceKeptUp(batches, skipped, dt),
                                         produced / dt, std::min(mp4, rtmp));
                allMinutesPassed &= noDrops;
                ++minute;
                std::printf("minute=%u elapsed_s=%.3f capture_fps=%.3f,%.3f paired_pct=%.3f render_fps=%.3f render_ms_mean=%.3f infer_hz=%.3f infer_batches=%llu infer_p99_ms=%.3f mp4_fps=%.3f rtmp_fps=%.3f no_drop=%s capture_drops=%llu,%llu capture_gaps=%llu,%llu queues=%zu,%zu analysis_skipped=%llu clip_decode_ms_mean=%.3f clip_frames=%llu\n",
                    minute, std::chrono::duration<double>(now-started).count(), captured[0]/dt, captured[1]/dt,
                    coverage, produced/dt, minuteRenderCount ? minuteRenderMs/minuteRenderCount : 0., batches/dt,
                    static_cast<unsigned long long>(batches), percentile(latencies, .99), mp4, rtmp,
                    noDrops ? "PASS" : "FAIL", static_cast<unsigned long long>(drops[0]),
                    static_cast<unsigned long long>(drops[1]), static_cast<unsigned long long>(gaps[0]),
                    static_cast<unsigned long long>(gaps[1]), queue, ring.size(),
                    static_cast<unsigned long long>(skipped),
                    clip.frames() > 0 ? (clip.decodeMs()-minuteClipMs)/std::max<uint64_t>(1, clip.frames()-minuteClipFrames) : 0.,
                    static_cast<unsigned long long>(clip.frames()));
                std::fflush(stdout);
                minuteClipFrames = clip.frames(); minuteClipMs = clip.decodeMs();
                minuteEncoded = counts;
                minuteEncodePending = mp4Pending;
                minuteEncodedPending = rtmpPending;
                minuteAt = now;
                minuteRenderMs = 0; minuteRenderCount = 0;
                nextMinute = started + std::chrono::seconds(kStartupSeconds + (minute + 1) * 60);
            }
            if (Clock::now() >= nextReport) {
                encoder.errors();
                if (!steadyBaselineSet && Clock::now() - started >= std::chrono::seconds(kStartupSeconds)) {
                    steadyEncodedBaseline = encoder.counts()[1];
                    steadyBaselineSet = true;
                }
                std::lock_guard<std::mutex> lock(shared.mutex);
                const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
                const auto encoded = encoder.counts();
                const double reportSeconds = std::chrono::duration<double>(Clock::now() - reportAt).count();
                std::printf("mp4_interval_fps=%.3f rtmp_interval_fps=%.3f\n",
                            (encoded[1] - reportCounts[1]) / reportSeconds,
                            (encoded[3] - reportCounts[3]) / reportSeconds);
                reportCounts = encoded;
                reportAt = Clock::now();
                std::printf("encoded_frames=%llu encoded_fps=%.3f rtmp_input_frames=%llu rtmp_input_fps=%.3f rtmp_frames=%llu rtmp_fps=%.3f render_misses=%llu analysis_skipped=%llu timestamp_gaps=%llu,%llu capture_id_gaps=%llu,%llu internal_gaps=%llu,%llu reordered=%llu,%llu\n",
                            static_cast<unsigned long long>(encoded[1]), encoded[1] / seconds,
                            static_cast<unsigned long long>(encoded[2]), encoded[2] / seconds,
                            static_cast<unsigned long long>(encoded[3]), encoded[3] / seconds,
                            static_cast<unsigned long long>(renderMisses),
                            static_cast<unsigned long long>(analysisSkipped.load()),
                            static_cast<unsigned long long>(shared.timestampGaps[0]),
                            static_cast<unsigned long long>(shared.timestampGaps[1]),
                            static_cast<unsigned long long>(shared.captureGaps[0]),
                            static_cast<unsigned long long>(shared.captureGaps[1]),
                            static_cast<unsigned long long>(shared.internalGaps[0]),
                            static_cast<unsigned long long>(shared.internalGaps[1]),
                            static_cast<unsigned long long>(shared.reordered[0]),
                            static_cast<unsigned long long>(shared.reordered[1]));
                std::printf("elapsed_s=%.1f capture=%llu,%llu capture_fps=%.2f,%.2f pair=%llu render=%llu infer_batches=%llu infer_hz=%.2f ring=%zu queue_drop=%llu,%llu unmatched=%llu,%llu acquire_timeouts=%llu,%llu stage_ms_render=%.2f prep=%.2f infer=%.2f\n",
                            seconds, static_cast<unsigned long long>(shared.received[0]),
                            static_cast<unsigned long long>(shared.received[1]),
                            shared.received[0] / seconds, shared.received[1] / seconds,
                            static_cast<unsigned long long>(paired), static_cast<unsigned long long>(rendered),
                            static_cast<unsigned long long>(inferred.load()), inferred.load() / seconds, ring.size(),
                            static_cast<unsigned long long>(shared.drops[0]),
                            static_cast<unsigned long long>(shared.drops[1]),
                            static_cast<unsigned long long>(unmatched[0]),
                            static_cast<unsigned long long>(unmatched[1]),
                            static_cast<unsigned long long>(shared.timeouts[0]),
                            static_cast<unsigned long long>(shared.timeouts[1]), renderMs, prepMs.load(), inferMs.load());
                nextReport = Clock::now() + std::chrono::seconds(1);
            }
        }
        check(!shared.failed, "capture/analysis worker failed");
    } catch (...) { failure = std::current_exception(); }
    shared.stop = true;
    {
        std::lock_guard<std::mutex> lock(analysisMutex);
        analysisStop = true;
        analysisLatest.reset();
    }
    analysisReady.notify_one();
    if (analysisWorker.joinable()) analysisWorker.join();
    if (analysisFailure && !failure) failure = analysisFailure;
    for (auto& cam : cameras) {
        if (cam.capture) cam.capture->stopRepeat();
        if (cam.output) cam.output->endOfStream();
    }
    for (auto& thread : consumers) if (thread.joinable()) thread.join();
    for (int i = 0; i < 2; ++i)
        for (auto& frame : shared.queued[i])
            if (cameras[i].output->releaseBuffer(frame.buffer) != STATUS_OK && !failure)
                failure = std::make_exception_ptr(std::runtime_error("queued frame release failed"));
    const size_t tailOmitted = ring.size();
    ring.clear();
    const auto stopped = Clock::now();
    const double seconds = std::chrono::duration<double>(stopped - started).count();
    const auto beforeEos = encoder.counts();
    try { encoder.finish(); }
    catch (...) { if (!failure) failure = std::current_exception(); }
    const auto encoded = encoder.counts();
    const double steadySeconds = std::max(0.0, seconds - kStartupSeconds);
    const double steadyEncodedFps = steadySeconds && steadyBaselineSet ?
        (encoded[1] - std::min(encoded[1], steadyEncodedBaseline)) / steadySeconds : 0.0;
    const bool valid = !failure && seconds >= options.seconds && steadySeconds > 0 &&
        minute >= static_cast<unsigned>(std::max(0, (options.seconds - kStartupSeconds) / 60)) &&
        (options.seconds < 5400 || minute >= 90) && allMinutesPassed &&
        maxRing >= kRingPairs && encoded[1] > 0 && encoded[1] == rendered &&
        encoded[3] == rendered && encoded[0] == rendered && encoded[2] == rendered &&
        steadyAcceptance(
            noDropVerdict(shared.windowTimestampGaps[1][0] + shared.windowTimestampGaps[1][1],
                          shared.windowCaptureGaps[1][0] + shared.windowCaptureGaps[1][1],
                          shared.windowInternalGaps[1][0] + shared.windowInternalGaps[1][1],
                          shared.windowReordered[1][0] + shared.windowReordered[1][1],
                          shared.windowDrops[1][0] + shared.windowDrops[1][1],
                          windowUnmatched[1], windowMisses[1]),
            shared.windowReceived[1][0] / steadySeconds,
            shared.windowReceived[1][1] / steadySeconds,
            inferenceKeptUp(windowInferred[1].load(), windowAnalysisSkipped[1], steadySeconds),
            windowRendered[1] / steadySeconds,
            std::min(steadyEncodedFps, steadySeconds ? encoded[3] / steadySeconds : 0.));
    for (size_t window = 0; window < 2; ++window) {
        const double duration = window == 0 ? std::min(seconds, double(kStartupSeconds)) :
                                std::max(0.0, seconds - kStartupSeconds);
        const auto possible = std::min(shared.windowReceived[window][0], shared.windowReceived[window][1]);
        const double coverage = possible ? 100.0 * windowPairs[window] / possible : 0.0;
        const uint64_t timestampGaps = shared.windowTimestampGaps[window][0] + shared.windowTimestampGaps[window][1];
        const uint64_t captureGaps = shared.windowCaptureGaps[window][0] + shared.windowCaptureGaps[window][1];
        const uint64_t internalGaps = shared.windowInternalGaps[window][0] + shared.windowInternalGaps[window][1];
        const uint64_t reordered = shared.windowReordered[window][0] + shared.windowReordered[window][1];
        const uint64_t queueDrops = shared.windowDrops[window][0] + shared.windowDrops[window][1];
        const bool noDrops = noDropVerdict(timestampGaps, captureGaps, internalGaps, reordered,
                                           queueDrops, windowUnmatched[window], windowMisses[window]);
        std::printf("window=%s elapsed_s=%.3f mode=%dx%d capture_fps=%.3f,%.3f capture=%llu,%llu pair=%llu paired_pct=%.3f render_fps=%.3f render=%llu infer_hz=%.3f infer_batches=%llu inference_latency_ms_p99=%.3f timestamp_gaps=%llu capture_id_gaps=%llu internal_gaps=%llu reordered=%llu queue_drops=%llu unmatched=%llu render_misses=%llu analysis_skipped=%llu no_drop=%s\n",
                    window == 0 ? "startup" : "steady", duration, options.width, options.height,
                    duration ? shared.windowReceived[window][0] / duration : 0.0,
                    duration ? shared.windowReceived[window][1] / duration : 0.0,
                    static_cast<unsigned long long>(shared.windowReceived[window][0]),
                    static_cast<unsigned long long>(shared.windowReceived[window][1]),
                    static_cast<unsigned long long>(windowPairs[window]), coverage,
                    duration ? windowRendered[window] / duration : 0.0,
                    static_cast<unsigned long long>(windowRendered[window]),
                    duration ? windowInferred[window].load() / duration : 0.0,
                    static_cast<unsigned long long>(windowInferred[window].load()),
                    percentile(inferenceLatencies[window], .99),
                    static_cast<unsigned long long>(timestampGaps), static_cast<unsigned long long>(captureGaps),
                    static_cast<unsigned long long>(internalGaps), static_cast<unsigned long long>(reordered),
                    static_cast<unsigned long long>(queueDrops), static_cast<unsigned long long>(windowUnmatched[window]),
                    static_cast<unsigned long long>(windowMisses[window]),
                    static_cast<unsigned long long>(windowAnalysisSkipped[window]), noDrops ? "PASS" : "FAIL");
    }
    std::printf("final elapsed_s=%.3f mode=%dx%d encoded_frames=%llu encoded_fps=%.3f steady_encoded_fps=%.3f rtmp_input_frames=%llu rtmp_frames=%llu encoder_queue_or_branch_gap=%llu render=%llu infer_batches=%llu infer_latency_ms_p99=%.3f max_ring=%llu tail_ring_omitted=%zu tail_policy=not_encoded encoded_before_eos=%llu acceptance=%s\n",
                seconds, options.width, options.height, static_cast<unsigned long long>(encoded[1]),
                seconds ? encoded[1] / seconds : 0.0, steadyEncodedFps,
                static_cast<unsigned long long>(encoded[2]), static_cast<unsigned long long>(encoded[3]),
                static_cast<unsigned long long>(rendered > encoded[1] ? rendered - encoded[1] : 0),
                static_cast<unsigned long long>(rendered), static_cast<unsigned long long>(inferred.load()),
                percentile(inferenceLatencies[1], .99), static_cast<unsigned long long>(maxRing), tailOmitted,
                static_cast<unsigned long long>(beforeEos[1]), valid ? "PASS" : "FAIL");
    if (failure) std::rethrow_exception(failure);
    check(valid, "full-duration capture/render/inference/two-encoder acceptance failed; inspect minute counters");
    return 0;
}
int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& e) { std::fprintf(stderr, "e2e spike failed: %s\n", e.what()); return 1; }
}
#endif
