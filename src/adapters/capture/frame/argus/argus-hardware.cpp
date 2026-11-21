#include <Argus/Argus.h>
#include <EGL/egl.h>
#include <cuda.h>
#include <cudaEGL.h>
#include <cuda_runtime_api.h>
#include <nvbufsurface.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "adapters/capture/frame/argus/direct-capture.hpp"

namespace sst::capture::argus {
namespace {
using Argus::AE_ANTIBANDING_MODE_60HZ;
using Argus::AWB_MODE_OFF;
using Argus::BayerTuple;
using Argus::Buffer;
using Argus::BUFFER_TYPE_EGL_IMAGE;
using Argus::BufferSettings;
using Argus::CameraDevice;
using Argus::CameraProvider;
using Argus::CAPTURE_INTENT_VIDEO_RECORD;
using Argus::CaptureSession;
using Argus::DENOISE_MODE_HIGH_QUALITY;
using Argus::EDGE_ENHANCE_MODE_FAST;
using Argus::IAutoControlSettings;
using Argus::IBuffer;
using Argus::IBufferOutputStream;
using Argus::IBufferOutputStreamSettings;
using Argus::ICameraProperties;
using Argus::ICameraProvider;
using Argus::ICaptureMetadata;
using Argus::ICaptureSession;
using Argus::IDenoiseSettings;
using Argus::IEdgeEnhanceSettings;
using Argus::IEGLImageBufferSettings;
using Argus::interface_cast;
using Argus::IRequest;
using Argus::ISensorMode;
using Argus::ISourceSettings;
using Argus::OutputStream;
using Argus::OutputStreamSettings;
using Argus::Range;
using Argus::Request;
using Argus::SensorMode;
using Argus::Status;
using Argus::STATUS_OK;
using Argus::STATUS_TIMEOUT;
using Argus::STREAM_TYPE_BUFFER;
using Argus::UniqueObj;

constexpr std::uint64_t kAcquireTimeoutNs = 200'000'000;
constexpr std::size_t kSampleRows = 4;
constexpr std::size_t kSampleColumns = 8;
constexpr std::size_t kPlaneSamples = kSampleRows * kSampleColumns;
constexpr float kPixelMax = 255.F;
constexpr float kChromaNeutral = 128.F;
auto Check(bool valid) -> void {
    if (!valid) {
        throw std::runtime_error("Argus capture operation failed");
    }
}
struct Slot {
    NvBufSurface* surface{nullptr};
    EGLImageKHR image{EGL_NO_IMAGE_KHR};
    UniqueObj<Buffer> buffer;
    CUgraphicsResource resource{nullptr};
    ~Slot() {
        if (resource != nullptr) {
            cuGraphicsUnregisterResource(resource);
        }
        buffer.reset();
        if (surface != nullptr) {
            if (image != EGL_NO_IMAGE_KHR) {
                NvBufSurfaceUnMapEglImage(surface, 0);
            }
            NvBufSurfaceDestroy(surface);
        }
    }
};
struct Camera {
    UniqueObj<CaptureSession> session;
    UniqueObj<OutputStream> stream;
    UniqueObj<Request> request;
    std::vector<std::unique_ptr<Slot>> slots;
    IBufferOutputStream* output{nullptr};
    ICaptureSession* capture{nullptr};
    ISourceSettings* source{nullptr};
    IAutoControlSettings* auto_control{nullptr};
};
class ArgusHardware final : public Hardware {
   public:
    ArgusHardware() : provider_(CameraProvider::create()) {
        auto* api = interface_cast<ICameraProvider>(provider_);
        Check(api != nullptr && api->getCameraDevices(&devices_) == STATUS_OK &&
              devices_.size() >= 2);
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        Check(display_ != EGL_NO_DISPLAY && eglInitialize(display_, nullptr, nullptr) == EGL_TRUE);
    }
    ~ArgusHardware() override {
        for (int i = 0; i < 2; ++i) {
            Close(i);
        }
        if (display_ != EGL_NO_DISPLAY) {
            eglTerminate(display_);
        }
    }
    auto Modes(int camera) -> std::vector<std::uint64_t> override {
        std::vector<SensorMode*> modes;
        Check(interface_cast<ICameraProperties>(devices_.at(static_cast<std::size_t>(camera)))
                  ->getAllSensorModes(&modes) == STATUS_OK);
        std::vector<std::uint64_t> result;
        for (auto* mode : modes) {
            auto* info = interface_cast<ISensorMode>(mode);
            if (info != nullptr && info->getResolution().width() == kSensorWidth &&
                info->getResolution().height() == kSensorHeight) {
                result.push_back(info->getFrameDurationRange().min());
            }
        }
        return result;
    }
    auto Open(int camera_index, const Settings& settings) -> void override {
        auto& camera = cameras_.at(static_cast<std::size_t>(camera_index));
        auto* api = interface_cast<ICameraProvider>(provider_);
        camera.session.reset(
            api->createCaptureSession(devices_.at(static_cast<std::size_t>(camera_index))));
        camera.capture = interface_cast<ICaptureSession>(camera.session);
        Check(camera.capture != nullptr);
        UniqueObj<OutputStreamSettings> stream_settings(
            camera.capture->createOutputStreamSettings(STREAM_TYPE_BUFFER));
        auto* buffer_settings = interface_cast<IBufferOutputStreamSettings>(stream_settings);
        Check(buffer_settings != nullptr &&
              buffer_settings->setBufferType(BUFFER_TYPE_EGL_IMAGE) == STATUS_OK);
        buffer_settings->setMetadataEnable(true);
        camera.stream.reset(camera.capture->createOutputStream(stream_settings.get()));
        camera.output = interface_cast<IBufferOutputStream>(camera.stream);
        Check(camera.output != nullptr);
        UniqueObj<BufferSettings> image_settings(camera.output->createBufferSettings());
        auto* egl_settings = interface_cast<IEGLImageBufferSettings>(image_settings);
        Check(egl_settings != nullptr && egl_settings->setEGLDisplay(display_) == STATUS_OK);
        for (int i = 0; i < settings.buffer_count; ++i) {
            auto slot = std::make_unique<Slot>();
            NvBufSurfaceAllocateParams params{};
            params.params.width = static_cast<std::uint32_t>(settings.width);
            params.params.height = static_cast<std::uint32_t>(settings.height);
            params.params.layout = NVBUF_LAYOUT_PITCH;
            params.params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
            params.params.memType = NVBUF_MEM_SURFACE_ARRAY;
            params.memtag = NvBufSurfaceTag_CAMERA;
            Check(NvBufSurfaceAllocate(&slot->surface, 1, &params) == 0);
            Check(NvBufSurfaceMapEglImage(slot->surface, 0) == 0);
            slot->image =
                static_cast<EGLImageKHR>(slot->surface->surfaceList[0].mappedAddr.eglImage);
            Check(slot->image != EGL_NO_IMAGE_KHR &&
                  egl_settings->setEGLImage(slot->image) == STATUS_OK);
            slot->buffer.reset(camera.output->createBuffer(image_settings.get()));
            auto* wrapped = interface_cast<IBuffer>(slot->buffer);
            Check(wrapped != nullptr);
            wrapped->setClientData(slot.get());
            camera.slots.push_back(std::move(slot));
            Check(camera.output->releaseBuffer(camera.slots.back()->buffer.get()) == STATUS_OK);
        }
        camera.request.reset(camera.capture->createRequest(CAPTURE_INTENT_VIDEO_RECORD));
        auto* request = interface_cast<IRequest>(camera.request);
        camera.source = request != nullptr
                            ? interface_cast<ISourceSettings>(request->getSourceSettings())
                            : nullptr;
        camera.auto_control =
            request != nullptr
                ? interface_cast<IAutoControlSettings>(request->getAutoControlSettings())
                : nullptr;
        auto* denoise = interface_cast<IDenoiseSettings>(camera.request);
        auto* edge = interface_cast<IEdgeEnhanceSettings>(camera.request);
        Check(request != nullptr && camera.source != nullptr && camera.auto_control != nullptr &&
              denoise != nullptr && edge != nullptr);
        std::vector<SensorMode*> modes;
        Check(interface_cast<ICameraProperties>(devices_.at(static_cast<std::size_t>(camera_index)))
                  ->getAllSensorModes(&modes) == STATUS_OK);
        auto* selected = static_cast<SensorMode*>(nullptr);
        for (auto* mode : modes) {
            auto* info = interface_cast<ISensorMode>(mode);
            if (info != nullptr && info->getResolution().width() == kSensorWidth &&
                info->getResolution().height() == kSensorHeight &&
                info->getFrameDurationRange().min() == settings.duration_ns) {
                selected = mode;
                break;
            }
        }
        Check(selected != nullptr &&
              request->enableOutputStream(camera.stream.get()) == STATUS_OK &&
              camera.source->setSensorMode(selected) == STATUS_OK &&
              camera.source->setFrameDurationRange(Range<std::uint64_t>(settings.duration_ns)) ==
                  STATUS_OK &&
              camera.auto_control->setAeAntibandingMode(AE_ANTIBANDING_MODE_60HZ) == STATUS_OK &&
              camera.auto_control->setAwbMode(AWB_MODE_OFF) == STATUS_OK &&
              denoise->setDenoiseMode(DENOISE_MODE_HIGH_QUALITY) == STATUS_OK &&
              denoise->setDenoiseStrength(settings.denoise_strength) == STATUS_OK &&
              edge->setEdgeEnhanceMode(EDGE_ENHANCE_MODE_FAST) == STATUS_OK &&
              edge->setEdgeEnhanceStrength(settings.edge_strength) == STATUS_OK);
        Apply(camera_index, Controls{});
    }
    auto Acquire(int camera_index) -> std::optional<Sample> override {
        auto& camera = cameras_.at(static_cast<std::size_t>(camera_index));
        Status status = STATUS_OK;
        auto* buffer = camera.output->acquireBuffer(kAcquireTimeoutNs, &status);
        if (buffer == nullptr) {
            if (status == STATUS_TIMEOUT) {
                return std::nullopt;
            }
            throw std::runtime_error("Argus buffer acquire failed");
        }
        auto* wrapped = interface_cast<IBuffer>(buffer);
        const auto* metadata = wrapped != nullptr
                                   ? interface_cast<const ICaptureMetadata>(wrapped->getMetadata())
                                   : nullptr;
        if (metadata == nullptr || metadata->getSensorTimestamp() == 0) {
            camera.output->releaseBuffer(buffer);
            throw std::runtime_error("Argus sensor timestamp unavailable");
        }
        const auto* slot = static_cast<const Slot*>(wrapped->getClientData());
        if (slot == nullptr) {
            camera.output->releaseBuffer(buffer);
            throw std::runtime_error("Argus buffer slot unavailable");
        }
        if (!cuda_initialized_) {
            if (cudaFree(nullptr) != cudaSuccess) {
                camera.output->releaseBuffer(buffer);
                throw std::runtime_error("CUDA context unavailable");
            }
            cuda_initialized_ = true;
        }
        auto* mutable_slot = const_cast<Slot*>(slot);
        if (mutable_slot->resource == nullptr &&
            cuGraphicsEGLRegisterImage(&mutable_slot->resource, slot->image,
                                       CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE) != CUDA_SUCCESS) {
            camera.output->releaseBuffer(buffer);
            throw std::runtime_error("CUDA EGL registration failed");
        }
        CUeglFrame frame{};
        if (cuGraphicsResourceGetMappedEglFrame(&frame, mutable_slot->resource, 0, 0) !=
                CUDA_SUCCESS ||
            frame.frameType != CU_EGL_FRAME_TYPE_PITCH || frame.planeCount < 2 ||
            frame.frame.pPitch[0] == nullptr || frame.frame.pPitch[1] == nullptr ||
            slot->surface->surfaceList[0].planeParams.pitch[0] < kSensorWidth ||
            slot->surface->surfaceList[0].planeParams.pitch[1] < kSensorWidth) {
            camera.output->releaseBuffer(buffer);
            throw std::runtime_error("CUDA EGL frame unavailable");
        }
        std::array<std::uint8_t, 2 * kPlaneSamples> pixels{};
        for (std::size_t plane = 0; plane < 2; ++plane) {
            CUDA_MEMCPY2D copy{};
            copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
            copy.srcDevice = reinterpret_cast<CUdeviceptr>(frame.frame.pPitch[plane]);
            copy.srcPitch = slot->surface->surfaceList[0].planeParams.pitch[plane];
            copy.dstMemoryType = CU_MEMORYTYPE_HOST;
            copy.dstHost = pixels.data() + plane * kPlaneSamples;
            copy.dstPitch = kSampleColumns;
            copy.WidthInBytes = kSampleColumns;
            copy.Height = kSampleRows;
            if (cuMemcpy2D(&copy) != CUDA_SUCCESS) {
                camera.output->releaseBuffer(buffer);
                throw std::runtime_error("CUDA EGL sample failed");
            }
        }
        float luma = 0;
        float red = 0;
        float blue = 0;
        for (std::size_t index = 0; index < kPlaneSamples; ++index) {
            luma +=
                static_cast<float>(pixels[index]) / (static_cast<float>(kPlaneSamples) * kPixelMax);
        }
        for (std::size_t index = 0; index < kPlaneSamples; index += 2) {
            blue += static_cast<float>(pixels[kPlaneSamples + index]) /
                    ((static_cast<float>(kPlaneSamples) * kChromaNeutral / 2));
            red += static_cast<float>(pixels[kPlaneSamples + index + 1]) /
                   ((static_cast<float>(kPlaneSamples) * kChromaNeutral / 2));
        }
        const auto index = static_cast<int>(std::distance(
            camera.slots.begin(),
            std::find_if(camera.slots.begin(), camera.slots.end(),
                         [slot](const auto& candidate) { return candidate.get() == slot; })));
        if (index >= static_cast<int>(camera.slots.size())) {
            camera.output->releaseBuffer(buffer);
            throw std::runtime_error("Argus buffer slot mismatch");
        }
        const auto wb_gains = metadata->getAwbGains();
        return Sample{.slot = index,
                      .timestamp_ns = metadata->getSensorTimestamp(),
                      .luma = luma,
                      .red = red,
                      .blue = blue,
                      .exposure_ns = metadata->getSensorExposureTime(),
                      .analog_gain = metadata->getSensorAnalogGain(),
                      .isp_gain = metadata->getIspDigitalGain(),
                      .wb_red = wb_gains.r(),
                      .wb_blue = wb_gains.b()};
    }
    auto Release(std::pair<int, int> camera_slot) -> void override {
        const auto [camera_index, slot] = camera_slot;
        auto& camera = cameras_.at(static_cast<std::size_t>(camera_index));
        if (camera.output != nullptr) {
            auto* buffer = camera.slots.at(static_cast<std::size_t>(slot))->buffer.get();
            Check(camera.output->releaseBuffer(buffer) == STATUS_OK);
        }
    }
    auto Apply(int camera_index, const Controls& controls) -> void override {
        auto& camera = cameras_.at(static_cast<std::size_t>(camera_index));
        Check(camera.source->setExposureTimeRange(Range<std::uint64_t>(controls.exposure_ns)) ==
                  STATUS_OK &&
              camera.source->setGainRange(Range<float>(controls.analog_gain)) == STATUS_OK &&
              camera.auto_control->setIspDigitalGainRange(Range<float>(controls.isp_gain)) ==
                  STATUS_OK &&
              camera.auto_control->setWbGains(
                  BayerTuple<float>(controls.wb_red, 1.F, 1.F, controls.wb_blue)) == STATUS_OK &&
              camera.capture->repeat(camera.request.get()) == STATUS_OK);
    }
    auto Close(int camera_index) -> void override {
        auto& camera = cameras_.at(static_cast<std::size_t>(camera_index));
        if (camera.capture != nullptr) {
            camera.capture->stopRepeat();
            camera.capture->waitForIdle();
        }
        if (camera.output != nullptr) {
            camera.output->endOfStream();
        }
        camera.request.reset();
        camera.slots.clear();
        camera.stream.reset();
        camera.session.reset();
        camera.output = nullptr;
        camera.capture = nullptr;
        camera.source = nullptr;
        camera.auto_control = nullptr;
    }

   private:
    UniqueObj<CameraProvider> provider_;
    std::vector<CameraDevice*> devices_;
    EGLDisplay display_{EGL_NO_DISPLAY};
    bool cuda_initialized_{false};
    std::array<Camera, 2> cameras_;
};
}  // namespace
extern "C" auto CreateArgusHardware() -> Hardware* { return new ArgusHardware(); }
}  // namespace sst::capture::argus
