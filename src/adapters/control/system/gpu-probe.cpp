#include "adapters/control/system/gpu-probe.hpp"

#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <thread>

namespace sst::adapters::control {

auto RunGpuProbe() -> int {
    void* driver = ::dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (driver == nullptr) {
        return 1;
    }
    using CudaInit = int (*)(unsigned int);
    using CudaCount = int (*)(int*);
    auto init = reinterpret_cast<CudaInit>(::dlsym(driver, "cuInit"));
    auto count = reinterpret_cast<CudaCount>(::dlsym(driver, "cuDeviceGetCount"));
    int devices = 0;
    const bool available =
        init != nullptr && count != nullptr && init(0) == 0 && count(&devices) == 0 && devices > 0;
    ::dlclose(driver);
    return available ? 0 : 1;
}

auto GpuAvailable() -> bool {
    pid_t child = -1;
    constexpr std::size_t kExecutablePathSize = 4096;
    std::array<char, kExecutablePathSize> executable{};
    const ssize_t length = ::readlink("/proc/self/exe", executable.data(), executable.size() - 1);
    if (length <= 0) {
        return false;
    }
    std::array<char, sizeof("--gpu-probe")> probe_flag{};
    std::copy_n("--gpu-probe", probe_flag.size(), probe_flag.data());
    std::array<char*, 3> args{executable.data(), probe_flag.data(), nullptr};
    if (::posix_spawn(&child, executable.data(), nullptr, nullptr, args.data(), environ) != 0) {
        return false;
    }
    constexpr auto kGpuTimeout = std::chrono::seconds{5};
    constexpr auto kPollInterval = std::chrono::milliseconds{50};
    const auto deadline = std::chrono::steady_clock::now() + kGpuTimeout;
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        if (::waitpid(child, &status, WNOHANG) == child) {
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        std::this_thread::sleep_for(kPollInterval);
    }
    ::kill(child, SIGKILL);
    ::waitpid(child, &status, 0);
    return false;
}

}  // namespace sst::adapters::control
