#include "adapters/network/monitor/thread-ethernet-carrier-monitor.hpp"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <utility>

#include "adapters/control/network/net-iface.hpp"

namespace sst::adapters::network {

ThreadEthernetCarrierMonitor::ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval)
    : ThreadEthernetCarrierMonitor(interval, EventSource{-1, 0}) {}

ThreadEthernetCarrierMonitor::ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval,
                                                           EventSource source)
    : interval_(interval), event_fd_(source.fd), interface_index_(source.interface_index) {}

ThreadEthernetCarrierMonitor::~ThreadEthernetCarrierMonitor() { Stop(); }

auto ThreadEthernetCarrierMonitor::Start(std::function<void()> on_poll) -> void {
    const std::lock_guard lock(run_mtx_);
    if (running_) {
        return;
    }
    on_poll_ = std::move(on_poll);
    running_ = true;
    thread_ = std::thread([this] { Loop(); });
}

auto ThreadEthernetCarrierMonitor::Stop() -> void {
    {
        const std::lock_guard lock(run_mtx_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

auto ThreadEthernetCarrierMonitor::OpenSocket() -> int {
    const int socket_fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (socket_fd < 0) {
        spdlog::warn("EthernetCarrierMonitor: netlink socket failed: {}", strerror(errno));
        return -1;
    }
    sockaddr_nl address{};
    address.nl_family = AF_NETLINK;
    address.nl_groups = RTMGRP_LINK;
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        spdlog::warn("EthernetCarrierMonitor: netlink bind failed: {}", strerror(errno));
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

auto ThreadEthernetCarrierMonitor::ReadEvent(EventSource source) -> bool {
    constexpr std::size_t kNetlinkBufferSize = 8192;
    std::array<char, kNetlinkBufferSize> buffer{};
    const auto bytes = recv(source.fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    if (bytes < 0) {
        return errno != EAGAIN && errno != EINTR;
    }
    if (bytes == static_cast<ssize_t>(buffer.size())) {
        return true;
    }
    int left = static_cast<int>(bytes);
    for (auto* header = reinterpret_cast<nlmsghdr*>(buffer.data()); NLMSG_OK(header, left);
         header = NLMSG_NEXT(header, left)) {
        if (header->nlmsg_type == NLMSG_ERROR) {
            return true;
        }
        if (header->nlmsg_type == RTM_NEWLINK &&
            header->nlmsg_len >= NLMSG_LENGTH(sizeof(ifinfomsg))) {
            const auto* link = reinterpret_cast<const ifinfomsg*>(NLMSG_DATA(header));
            if (static_cast<unsigned int>(link->ifi_index) == source.interface_index) {
                return true;
            }
        }
    }
    return false;
}

auto ThreadEthernetCarrierMonitor::Loop() -> void {
    int socket_fd = event_fd_ >= 0 ? event_fd_ : OpenSocket();
    unsigned int index = interface_index_;
    if (index == 0) {
        index = if_nametoindex(sst::adapters::control::DetectWiredIface().c_str());
    }
    auto next_check = std::chrono::steady_clock::now() + interval_;
    while (true) {
        {
            const std::lock_guard lock(run_mtx_);
            if (!running_) {
                break;
            }
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            next_check - std::chrono::steady_clock::now());
        const int timeout = static_cast<int>(std::max(0L, remaining.count()));
        pollfd watch{.fd = socket_fd, .events = POLLIN, .revents = 0};
        const int ready = poll(&watch, socket_fd >= 0 ? 1 : 0, std::min(timeout, 100));
        bool reconcile = ready < 0 && errno != EINTR;
        if (ready > 0 && (watch.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            reconcile = true;
            if (event_fd_ < 0) {
                close(socket_fd);
                socket_fd = -1;
            }
        } else if (ready > 0 && (watch.revents & POLLIN) != 0) {
            reconcile = ReadEvent({socket_fd, index});
        }
        if (std::chrono::steady_clock::now() >= next_check) {
            reconcile = true;
            next_check = std::chrono::steady_clock::now() + interval_;
        }
        if (reconcile && on_poll_) {
            on_poll_();
        }
    }
    if (event_fd_ < 0 && socket_fd >= 0) {
        close(socket_fd);
    }
}

}  // namespace sst::adapters::network
