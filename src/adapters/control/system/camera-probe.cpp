#include "adapters/control/system/camera-probe.hpp"

#include <gst/gst.h>
#include <spawn.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace sst::adapters::control {

namespace {
constexpr auto kProbeTimeout = std::chrono::seconds{10};
constexpr auto kPollInterval = std::chrono::milliseconds{50};
constexpr int kAbsentExitCode = 10;
constexpr int kRedirectFailedExitCode = 11;
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

auto RunCameraProbeChild(const std::vector<std::string>& argv,
                         std::chrono::seconds timeout) -> sst_cam::CameraHealth {
    if (argv.empty()) {
        return sst_cam::CAMERA_HEALTH_DOWN;
    }
    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        args.push_back(
            const_cast<char*>(arg.c_str()));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    args.push_back(nullptr);
    pid_t child = -1;
    const int spawned = ::posix_spawnp(&child, args[0], nullptr, nullptr, args.data(), environ);
    if (spawned != 0) {
        spdlog::warn("camera probe child: spawn failed: {}", spawned);
        return sst_cam::CAMERA_HEALTH_DOWN;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    int status = 0;
    while (true) {
        const pid_t done = ::waitpid(child, &status, WNOHANG);
        if (done == child) {
            if (WIFEXITED(status)) {
                const int code = WEXITSTATUS(status);
                if (code != 0 && code != kAbsentExitCode) {
                    spdlog::warn("camera probe child: exit code {}", code);
                }
                return code == 0 ? sst_cam::CAMERA_HEALTH_OK : sst_cam::CAMERA_HEALTH_DOWN;
            }
            spdlog::warn("camera probe child: signal {}",
                         WIFSIGNALED(status) ? WTERMSIG(status) : -1);
            return sst_cam::CAMERA_HEALTH_DOWN;
        }
        if (done < 0) {
            spdlog::warn("camera probe child: waitpid failed: {}", std::strerror(errno));
            return sst_cam::CAMERA_HEALTH_DOWN;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            spdlog::warn("camera probe child: timed out after {}s", timeout.count());
            ::kill(child, SIGKILL);
            ::waitpid(child, &status, 0);
            return sst_cam::CAMERA_HEALTH_DOWN;
        }
        std::this_thread::sleep_for(kPollInterval);
    }
}

auto ProbeCameras(std::array<std::int64_t, 2>* durations_ms) -> CameraStatuses {
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
        result[static_cast<std::size_t>(index)] = RunCameraProbeChild(
            {executable.data(), "--probe-cameras", std::to_string(index)}, kProbeTimeout);
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - start)
                                  .count();
        if (durations_ms != nullptr) {
            (*durations_ms)[static_cast<std::size_t>(index)] = duration;
        }
        spdlog::info("camera probe {}: {} in {} ms", index,
                     sst_cam::CameraHealth_Name(result[static_cast<std::size_t>(index)]), duration);
    }
    return result;
}

auto RunCameraProbe(int index) -> int {
    if (::dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
        return kRedirectFailedExitCode;
    }
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
        return kAbsentExitCode;
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
    return present ? 0 : kAbsentExitCode;
}

}  // namespace sst::adapters::control
