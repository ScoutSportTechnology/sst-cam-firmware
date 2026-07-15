#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "adapters/control/network/gratuitous-arp.hpp"

// Frame-construction tests for the gratuitous-ARP announcement. The raw-socket
// SEND path (SendGratuitousArp) is hardware/privilege-bound (needs CAP_NET_RAW +
// a real iface), so it only exercises on-device; here we assert the pure frame
// builder and the never-throw guards that run anywhere.

namespace {

using sst::adapters::control::BuildArpAnnouncement;
using sst::adapters::control::kMacLen;
using sst::adapters::control::SendGratuitousArp;

constexpr std::uint8_t kBroadcastByte = 0xFF;
constexpr std::array<std::uint8_t, kMacLen> kMac{0xF8, 0x3D, 0xC6, 0x1F, 0x8C, 0x71};

TEST(BuildArpAnnouncement, EthernetHeaderIsBroadcastArpFromIfaceMac) {
    std::uint32_t ip_be = 0;
    ASSERT_EQ(::inet_pton(AF_INET, "10.10.1.142", &ip_be), 1);
    const auto frame = BuildArpAnnouncement(kMac, ip_be);

    ether_header eth{};
    std::memcpy(&eth, frame.data(), sizeof(eth));

    // Destination MAC is broadcast; source MAC is the iface MAC.
    for (std::size_t i = 0; i < ETH_ALEN; ++i) {
        EXPECT_EQ(eth.ether_dhost[i], kBroadcastByte) << "dest MAC byte " << i;
        EXPECT_EQ(eth.ether_shost[i], kMac.at(i)) << "src MAC byte " << i;
    }
    EXPECT_EQ(ntohs(eth.ether_type), ETHERTYPE_ARP);
}

TEST(BuildArpAnnouncement, ArpPayloadIsAGratuitousAnnouncement) {
    std::uint32_t ip_be = 0;
    ASSERT_EQ(::inet_pton(AF_INET, "10.10.1.142", &ip_be), 1);
    const auto frame = BuildArpAnnouncement(kMac, ip_be);

    ether_arp arp{};
    std::memcpy(&arp, frame.data() + ETHER_HDR_LEN, sizeof(arp));

    EXPECT_EQ(ntohs(arp.ea_hdr.ar_hrd), ARPHRD_ETHER);
    EXPECT_EQ(ntohs(arp.ea_hdr.ar_pro), ETHERTYPE_IP);
    EXPECT_EQ(arp.ea_hdr.ar_hln, ETH_ALEN);
    EXPECT_EQ(arp.ea_hdr.ar_pln, sizeof(std::uint32_t));
    EXPECT_EQ(ntohs(arp.ea_hdr.ar_op), ARPOP_REQUEST);

    // Sender hardware == iface MAC.
    EXPECT_EQ(std::memcmp(arp.arp_sha, kMac.data(), ETH_ALEN), 0);
    // Sender protocol addr == target protocol addr == the STA IP (gratuitous).
    EXPECT_EQ(std::memcmp(arp.arp_spa, &ip_be, sizeof(ip_be)), 0);
    EXPECT_EQ(std::memcmp(arp.arp_tpa, &ip_be, sizeof(ip_be)), 0);
    // Target hardware is zeroed on an announcement.
    const std::array<std::uint8_t, kMacLen> zero{};
    EXPECT_EQ(std::memcmp(arp.arp_tha, zero.data(), ETH_ALEN), 0);
}

TEST(SendGratuitousArp, EmptyIfaceOrIpReturnsCleanly) {
    // No throw, no crash — the guards short-circuit before any syscall.
    EXPECT_NO_THROW(SendGratuitousArp("", "10.10.1.142"));
    EXPECT_NO_THROW(SendGratuitousArp("wlP1p1s0", ""));
}

TEST(SendGratuitousArp, MalformedIpReturnsCleanly) {
    // inet_pton rejects the address before any socket is opened.
    EXPECT_NO_THROW(SendGratuitousArp("wlP1p1s0", "not-an-ip"));
}

}  // namespace
