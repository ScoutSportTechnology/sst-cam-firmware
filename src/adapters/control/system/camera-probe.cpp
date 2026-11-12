#include "adapters/control/system/camera-probe.hpp"

#include <gst/gst.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "adapters/control/network/subprocess.hpp"

namespace sst::adapters::control {

namespace {
constexpr auto kProbeTimeout = std::chrono::seconds{10};
constexpr GstClockTime kArgusWait = 7 * GST_SECOND;
constexpr std::size_t kExecutablePathSize = 4096;

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto ArgusReachable() -> bool {
    const int socket_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) {
        return false;
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    constexpr std::string_view kSocket = "/tmp/argus_socket";
    std::memcpy(address.sun_path, kSocket.data(), kSocket.size() + 1);
    const bool reachable =
        ::connect(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    ::close(socket_fd);
    return reachable;
}

}  // namespace

auto DecodeCameraProbe(std::string_view output, bool exited_ok) -> CameraStatuses {
    CameraStatuses result{sst_cam::CAMERA_HEALTH_DOWN, sst_cam::CAMERA_HEALTH_DOWN};
    if (exited_ok && output.size() == 4 && (output[0] == '0' || output[0] == '1') &&
        output[1] == ' ' && (output[2] == '0' || output[2] == '1') && output[3] == '\n') {
        result[0] = output[0] == '1' ? sst_cam::CAMERA_HEALTH_OK : sst_cam::CAMERA_HEALTH_DOWN;
        result[1] = output[2] == '1' ? sst_cam::CAMERA_HEALTH_OK : sst_cam::CAMERA_HEALTH_DOWN;
    }
    return result;
}

auto ProbeCameras() -> CameraStatuses {
    CameraStatuses result{sst_cam::CAMERA_HEALTH_DOWN, sst_cam::CAMERA_HEALTH_DOWN};
    const char* missing = std::getenv("SST_PREFLIGHT_MISSING_CAMERA");
    if (missing != nullptr && std::strcmp(missing, "0") != 0 && std::strcmp(missing, "1") != 0) {
        return result;
    }
    if (!ArgusReachable()) {
        return result;
    }
    std::array<char, kExecutablePathSize> executable{};
    if (::readlink("/proc/self/exe", executable.data(), executable.size() - 1) <= 0) {
        return result;
    }
    for (int index = 0; index < 2; ++index) {
        if (missing != nullptr && missing[0] == '0' + index) {
            continue;
        }
        if (!std::filesystem::exists("/dev/video" + std::to_string(index))) {
            continue;
        }
        const auto start = std::chrono::steady_clock::now();
        const auto probe = CaptureBounded(
            {executable.data(), "--probe-cameras", std::to_string(index)}, kProbeTimeout);
        const auto decoded = DecodeCameraProbe(probe.output, probe.ok);
        result[static_cast<std::size_t>(index)] = decoded[static_cast<std::size_t>(index)];
        spdlog::info("camera probe {}: {} in {} ms", index,
                     sst_cam::CameraHealth_Name(result[static_cast<std::size_t>(index)]),
                     std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count());
    }
    return result;
}

auto RunCameraProbe(int index) -> int {
    gst_init(nullptr, nullptr);
    const std::string pipeline = "nvarguscamerasrc sensor-id=" + std::to_string(index) +
                                 " num-buffers=1 ! video/x-raw(memory:NVMM) ! fakesink";
    GError* error = nullptr;
    GstElement* element = gst_parse_launch(pipeline.c_str(), &error);
    const bool parse_failed = error != nullptr;
    if (error != nullptr) {
        g_error_free(error);
    }
    if (element == nullptr || parse_failed) {
        if (element != nullptr) {
            gst_object_unref(element);
        }
        return 1;
    }
    GstBus* bus = gst_element_get_bus(element);
    const auto state = gst_element_set_state(element, GST_STATE_PLAYING);
    GstMessage* message =
        state == GST_STATE_CHANGE_FAILURE
            ? nullptr
            : gst_bus_timed_pop_filtered(
                  bus, kArgusWait,
                  static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    const bool present = message != nullptr && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
    if (message != nullptr) {
        gst_message_unref(message);
    }
    gst_element_set_state(element, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(element);
    if (present) {
        std::printf("%d %d\n", index == 0 ? 1 : 0, index == 1 ? 1 : 0);
    }
    return present ? 0 : 1;
}

}  // namespace sst::adapters::control
