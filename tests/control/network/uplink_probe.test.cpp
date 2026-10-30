// Per-interface uplink reachability probe (U7 / R7). The real per-interface TCP
// connect is ON-DEVICE (SO_BINDTODEVICE needs root + a live NIC — bounded no-throw
// here). The logic tested here: the pure default-route parser, and the
// background-poller CACHE contract — the getter is served from a cache the poll
// thread refreshes, NEVER an inline per-request probe on the caller's thread
// (which on the BLE dispatcher would stall every command).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "adapters/control/network/ip-route-uplink-probe.hpp"
#include "domain/network/models/interface-reachability.hpp"

namespace {

using sst::adapters::control::HasDefaultRoute;
using sst::adapters::control::IpRouteUplinkProbe;
using sst::network::InterfaceReachability;

constexpr auto kPollInterval = std::chrono::milliseconds(20);
constexpr auto kSpinStep = std::chrono::milliseconds(5);
constexpr auto kSlowInterval = std::chrono::milliseconds(200);
constexpr int kGetterReads = 50;

template <typename Pred>
auto WaitFor(Pred pred) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(kSpinStep);
    }
    return pred();
}

TEST(UplinkProbeParserTest, HasDefaultRoute) {
    EXPECT_TRUE(HasDefaultRoute("default via 10.10.1.1 dev eth0 proto dhcp\n"));
    EXPECT_FALSE(HasDefaultRoute(""));
    EXPECT_FALSE(HasDefaultRoute("10.10.1.0/24 dev eth0 scope link\n"));
}

TEST(UplinkProbeTest, PollerRefreshesCacheFromTheSampler) {
    std::atomic<InterfaceReachability> latest{InterfaceReachability{}};
    IpRouteUplinkProbe probe([&latest] { return latest.load(); }, kPollInterval);
    probe.Start();

    // Cache starts empty (no reachability) until the first poll lands.
    EXPECT_FALSE(probe.HasInternetUplink());

    latest.store(InterfaceReachability{.ethernet_wan = true, .wifi_sta = false});
    EXPECT_TRUE(WaitFor([&] {
        return probe.ReachableInterfaces() ==
               InterfaceReachability{.ethernet_wan = true, .wifi_sta = false};
    }));
    EXPECT_TRUE(probe.HasInternetUplink());  // AnyReachable() over the cache

    // A later change is picked up on the interval.
    latest.store(InterfaceReachability{.ethernet_wan = false, .wifi_sta = true});
    EXPECT_TRUE(WaitFor([&] {
        return probe.ReachableInterfaces() ==
               InterfaceReachability{.ethernet_wan = false, .wifi_sta = true};
    }));
    probe.Stop();
}

// The getter is served from the CACHE — reading it does NOT invoke the sampler
// (no inline probe on the caller/dispatcher thread).
TEST(UplinkProbeTest, GetterReadsCacheAndDoesNotProbeInline) {
    std::atomic<int> sampler_calls{0};
    IpRouteUplinkProbe probe(
        [&sampler_calls] {
            ++sampler_calls;
            return InterfaceReachability{.ethernet_wan = true, .wifi_sta = false};
        },
        kSlowInterval);
    probe.Start();
    EXPECT_TRUE(WaitFor([&] { return probe.HasInternetUplink(); }));

    const int calls_after_warm = sampler_calls.load();
    // Many getter reads must not fork/probe — the sampler count stays put.
    for (int i = 0; i < kGetterReads; ++i) {
        (void)probe.ReachableInterfaces();
        (void)probe.HasInternetUplink();
    }
    EXPECT_EQ(sampler_calls.load(), calls_after_warm);
    probe.Stop();
}

TEST(UplinkProbeTest, StartIsIdempotentAndStopBeforeStartIsSafe) {
    std::atomic<int> calls{0};
    IpRouteUplinkProbe probe(
        [&calls] {
            ++calls;
            return InterfaceReachability{};
        },
        kPollInterval);
    probe.Stop();  // no-op before Start
    probe.Start();
    probe.Start();  // second Start is a no-op — must not spawn a second thread
    EXPECT_TRUE(WaitFor([&] { return calls.load() >= 1; }));
    probe.Stop();
    probe.Stop();  // idempotent
    SUCCEED();
}

// ON-DEVICE surface: the real per-interface probe is bounded + never throws in the
// container (no wired uplink, SO_BINDTODEVICE denied).
TEST(UplinkProbeTest, RealSamplerIsBoundedAndDoesNotThrow) {
    IpRouteUplinkProbe probe;  // default sampler = the real SO_BINDTODEVICE connects
    EXPECT_NO_THROW({
        probe.Start();
        (void)probe.ReachableInterfaces();
        probe.Stop();
    });
}

}  // namespace
