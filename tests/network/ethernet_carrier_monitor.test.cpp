#include <gtest/gtest.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "adapters/network/monitor/thread-ethernet-carrier-monitor.hpp"

namespace {

using namespace std::chrono_literals;

struct LinkEvent {
    nlmsghdr header{};
    ifinfomsg link{};
};

class EthernetCarrierMonitorTest : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets.data()), 0); }
    void TearDown() override {
        close(sockets[0]);
        close(sockets[1]);
    }
    auto SendLink(unsigned int flags) -> void {
        LinkEvent event{};
        event.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
        event.header.nlmsg_type = RTM_NEWLINK;
        event.link.ifi_index = kInterfaceIndex;
        event.link.ifi_flags = flags;
        ASSERT_EQ(send(sockets[1], &event, sizeof(event), 0), sizeof(event));
    }
    auto WaitFor(int count) -> bool {
        std::unique_lock lock(mutex);
        return cv.wait_for(lock, 1s, [&] { return calls >= count; });
    }
    auto Callback() -> void {
        std::lock_guard lock(mutex);
        ++calls;
        cv.notify_all();
    }
    static constexpr unsigned int kInterfaceIndex = 42;
    std::array<int, 2> sockets{};
    std::mutex mutex;
    std::condition_variable cv;
    int calls{0};
};

TEST_F(EthernetCarrierMonitorTest, LinkUpWakesArbiterWithinOneLoop) {
    sst::adapters::network::ThreadEthernetCarrierMonitor monitor{2s, {sockets[0], kInterfaceIndex}};
    monitor.Start([this] { Callback(); });
    SendLink(IFF_LOWER_UP);
    EXPECT_TRUE(WaitFor(1));
    monitor.Stop();
}

TEST_F(EthernetCarrierMonitorTest, LostEventConvergesOnFallbackTick) {
    sst::adapters::network::ThreadEthernetCarrierMonitor monitor{20ms,
                                                                 {sockets[0], kInterfaceIndex}};
    monitor.Start([this] { Callback(); });
    EXPECT_TRUE(WaitFor(1));
    monitor.Stop();
}

TEST_F(EthernetCarrierMonitorTest, NetlinkEnobufsTriggersResync) {
    sst::adapters::network::ThreadEthernetCarrierMonitor monitor{2s, {sockets[0], kInterfaceIndex}};
    monitor.Start([this] { Callback(); });
    struct {
        nlmsghdr header;
        nlmsgerr error;
    } event{};
    event.header.nlmsg_len = NLMSG_LENGTH(sizeof(nlmsgerr));
    event.header.nlmsg_type = NLMSG_ERROR;
    event.error.error = -ENOBUFS;
    ASSERT_EQ(send(sockets[1], &event, sizeof(event), 0), sizeof(event));
    EXPECT_TRUE(WaitFor(1));
    monitor.Stop();
}

TEST_F(EthernetCarrierMonitorTest, FastFlapReconcilesFinalLevel) {
    sst::adapters::network::ThreadEthernetCarrierMonitor monitor{2s, {sockets[0], kInterfaceIndex}};
    monitor.Start([this] { Callback(); });
    SendLink(IFF_LOWER_UP);
    SendLink(0);
    SendLink(IFF_LOWER_UP);
    EXPECT_TRUE(WaitFor(1));
    monitor.Stop();
}

}  // namespace
