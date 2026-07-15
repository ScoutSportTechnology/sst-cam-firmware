#include "adapters/control/network/gratuitous-arp.hpp"

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>

namespace sst::adapters::control {

namespace {

// How many announcements to fire back-to-back. A handful raises the odds one
// lands past a busy/duplicate-suppressing neighbour without stalling the caller
// (each sendto is non-blocking on a datagram AF_PACKET socket).
constexpr int kArpAnnounceCount = 3;

// Every octet of a broadcast address (dest MAC / sll_addr fill).
constexpr std::uint8_t kBroadcastByte = 0xFF;

// RAII guard for the raw socket fd so every early return closes it.
class FdGuard {
   public:
    explicit FdGuard(int raw_fd) : fd_(raw_fd) {}
    ~FdGuard() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    FdGuard(const FdGuard&) = delete;
    auto operator=(const FdGuard&) -> FdGuard& = delete;
    FdGuard(FdGuard&&) = delete;
    auto operator=(FdGuard&&) -> FdGuard& = delete;
    [[nodiscard]] auto Get() const -> int { return fd_; }

   private:
    int fd_;
};

}  // namespace

auto BuildArpAnnouncement(const std::array<std::uint8_t, kMacLen>& mac,
                          std::uint32_t ip_be) -> std::array<std::uint8_t, kArpFrameLen> {
    ether_header eth{};
    std::memset(eth.ether_dhost, kBroadcastByte, ETH_ALEN);  // broadcast
    std::memcpy(eth.ether_shost, mac.data(), ETH_ALEN);
    eth.ether_type = htons(ETHERTYPE_ARP);

    ether_arp arp{};
    arp.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    arp.ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    arp.ea_hdr.ar_hln = ETH_ALEN;
    arp.ea_hdr.ar_pln = sizeof(std::uint32_t);
    arp.ea_hdr.ar_op = htons(ARPOP_REQUEST);  // announcement uses REQUEST (RFC 5227)
    std::memcpy(arp.arp_sha, mac.data(), ETH_ALEN);
    std::memcpy(arp.arp_spa, &ip_be, sizeof(ip_be));
    // arp_tha left zeroed (announcement); target IP == sender IP (gratuitous).
    std::memcpy(arp.arp_tpa, &ip_be, sizeof(ip_be));

    std::array<std::uint8_t, kArpFrameLen> frame{};
    std::memcpy(frame.data(), &eth, sizeof(eth));
    std::memcpy(frame.data() + sizeof(eth), &arp, sizeof(arp));
    return frame;
}

auto SendGratuitousArp(const std::string& iface, const std::string& ip_str) -> void {
    if (iface.empty() || ip_str.empty()) {
        return;
    }

    in_addr addr{};
    if (::inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
        spdlog::debug("SendGratuitousArp: skip — '{}' is not an IPv4 address", ip_str);
        return;
    }

    const FdGuard sock(::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP)));
    if (sock.Get() < 0) {
        // Almost always a missing CAP_NET_RAW — best-effort, so log and move on.
        spdlog::info("SendGratuitousArp: raw socket unavailable ({}); skipping (best-effort)",
                     std::strerror(errno));
        return;
    }

    const unsigned int ifindex = ::if_nametoindex(iface.c_str());
    if (ifindex == 0) {
        spdlog::debug("SendGratuitousArp: unknown iface '{}'", iface);
        return;
    }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock.Get(), SIOCGIFHWADDR, &ifr) < 0) {
        spdlog::debug("SendGratuitousArp: cannot read MAC for '{}' ({})", iface,
                      std::strerror(errno));
        return;
    }
    std::array<std::uint8_t, kMacLen> mac{};
    std::memcpy(mac.data(), ifr.ifr_hwaddr.sa_data, mac.size());

    const auto frame = BuildArpAnnouncement(mac, addr.s_addr);

    sockaddr_ll dst{};
    dst.sll_family = AF_PACKET;
    dst.sll_ifindex = static_cast<int>(ifindex);
    dst.sll_halen = ETH_ALEN;
    std::memset(dst.sll_addr, kBroadcastByte, ETH_ALEN);  // broadcast

    int sent = 0;
    for (int i = 0; i < kArpAnnounceCount; ++i) {
        const ssize_t written = ::sendto(sock.Get(), frame.data(), frame.size(), 0,
                                         reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
        if (written == static_cast<ssize_t>(frame.size())) {
            ++sent;
        }
    }
    if (sent > 0) {
        spdlog::info("SendGratuitousArp: announced {} on {} ({}x)", ip_str, iface, sent);
    } else {
        spdlog::info("SendGratuitousArp: no announcement sent for {} on {} ({})", ip_str, iface,
                     std::strerror(errno));
    }
}

}  // namespace sst::adapters::control
