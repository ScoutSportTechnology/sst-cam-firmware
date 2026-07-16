#include "adapters/control/network/ip-route-uplink-probe.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

#include "adapters/control/network/net-iface.hpp"

namespace sst::adapters::control {

namespace {

// The reachability target: a well-known anycast address on the HTTPS port. A
// successful TCP handshake here means the bound interface has a working path to
// the public internet (not merely a link/route). Kept as an IP literal so the
// probe never depends on DNS.
constexpr const char* kReachabilityHost = "1.1.1.1";
constexpr std::uint16_t kReachabilityPort = 443;
constexpr int kConnectTimeoutMs = 2000;

// RAII fd closer so an early return never leaks the socket.
class ScopedFd {
   public:
    explicit ScopedFd(int descriptor) : fd_(descriptor) {}
    ~ScopedFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    ScopedFd(const ScopedFd&) = delete;
    auto operator=(const ScopedFd&) -> ScopedFd& = delete;
    ScopedFd(ScopedFd&&) = delete;
    auto operator=(ScopedFd&&) -> ScopedFd& = delete;
    [[nodiscard]] auto get() const -> int { return fd_; }

   private:
    int fd_;
};

// Non-blocking TCP connect bound to `iface` (SO_BINDTODEVICE), succeeding iff the
// handshake completes within the deadline. SO_BINDTODEVICE forces egress out the
// named interface regardless of the routing table, so this measures THAT
// interface's reachability even while the arbiter is mutating policy routing.
// Never throws; any error (empty iface, no CAP_NET_RAW, refused, timeout) => false.
auto TcpConnectReachable(const std::string& iface) -> bool {
    if (iface.empty()) {
        return false;
    }
    const int raw = ::socket(AF_INET, SOCK_STREAM, 0);
    if (raw < 0) {
        return false;
    }
    const ScopedFd sock(raw);
    if (::setsockopt(sock.get(), SOL_SOCKET, SO_BINDTODEVICE, iface.c_str(),
                     static_cast<socklen_t>(iface.size())) != 0) {
        return false;  // needs root/CAP_NET_RAW — true on-device, fails in the container
    }
    const int flags = ::fcntl(sock.get(), F_GETFL, 0);
    if (flags < 0 || ::fcntl(sock.get(), F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kReachabilityPort);
    if (::inet_pton(AF_INET, kReachabilityHost, &addr.sin_addr) != 1) {
        return false;
    }

    const int connect_result =
        ::connect(sock.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (connect_result == 0) {
        return true;  // immediate connect (rare)
    }
    if (errno != EINPROGRESS) {
        return false;
    }

    pollfd pfd{.fd = sock.get(), .events = POLLOUT, .revents = 0};
    if (::poll(&pfd, 1, kConnectTimeoutMs) <= 0) {
        return false;  // timeout or poll error
    }
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (::getsockopt(sock.get(), SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) {
        return false;
    }
    return so_error == 0;
}

}  // namespace

auto HasDefaultRoute(const std::string& ip_route_default_output) -> bool {
    // `ip route show default` prints one "default via <gw> dev <iface> ..." line
    // per default route, or nothing when there is none. Any "default" token means
    // the camera has an uplink route to the internet.
    return ip_route_default_output.find("default") != std::string::npos;
}

auto ProbeInterfaceReachability() -> sst::network::InterfaceReachability {
    const std::string wired = DetectWiredIface();
    const std::string sta = DetectWifiStaIface();
    return {.ethernet_wan = TcpConnectReachable(wired), .wifi_sta = TcpConnectReachable(sta)};
}

IpRouteUplinkProbe::IpRouteUplinkProbe(ReachabilitySampler sampler,
                                       std::chrono::milliseconds interval)
    : sampler_(std::move(sampler)), interval_(interval) {}

IpRouteUplinkProbe::~IpRouteUplinkProbe() { Stop(); }

auto IpRouteUplinkProbe::ReachableInterfaces() -> sst::network::InterfaceReachability {
    const std::lock_guard lock(cache_mtx_);
    return cache_;
}

auto IpRouteUplinkProbe::SampleOnce() -> void {
    sst::network::InterfaceReachability sampled =
        sampler_ ? sampler_() : sst::network::InterfaceReachability{};
    const std::lock_guard lock(cache_mtx_);
    cache_ = sampled;
}

auto IpRouteUplinkProbe::Start() -> void {
    // Assign thread_ while still holding run_mtx_ so a concurrent Stop() can never
    // observe running_==true with an unassigned thread_ (which would leave the Loop
    // thread joinable-but-never-joined -> std::terminate at destruction). The first
    // sample runs inside Loop(), so Start() never blocks the caller on a slow
    // connect at boot (telemetry-poller learning).
    const std::lock_guard lock(run_mtx_);
    if (running_) {
        return;
    }
    running_ = true;
    thread_ = std::thread([this] { Loop(); });
}

auto IpRouteUplinkProbe::Stop() -> void {
    {
        const std::lock_guard lock(run_mtx_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    run_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

auto IpRouteUplinkProbe::Loop() -> void {
    std::unique_lock lock(run_mtx_);
    while (running_) {
        lock.unlock();
        SampleOnce();  // sample immediately on the first pass, then every interval
        lock.lock();
        if (run_cv_.wait_for(lock, interval_, [this] { return !running_; })) {
            break;  // Stop() signalled; exit promptly
        }
    }
}

}  // namespace sst::adapters::control
