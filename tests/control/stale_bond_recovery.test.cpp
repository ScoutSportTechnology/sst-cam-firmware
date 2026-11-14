#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "adapters/control/ble/bluez/bluez-ble-transport.hpp"
#include "adapters/control/ble/bluez/stale-bond-recovery.hpp"

namespace {

using sst::adapters::control::DecodeAuthenticationFailure;
using sst::adapters::control::StaleBondRecovery;
using Clock = std::chrono::steady_clock;
constexpr auto kFailure = StaleBondRecovery::kAuthenticationFailed;
constexpr auto kBeforeCooldown = std::chrono::seconds{StaleBondRecovery::kCooldown.count() - 1};

TEST(StaleBondRecoveryTest, KernelMgmtAuthFailureDecodesTheCentralAndRejectsOtherEvents) {
    const std::vector<std::uint8_t> event{0x11, 0,    0,    0,    8,    0, 0xF5,
                                          0xBE, 0xCC, 0x4B, 0xCF, 0x04, 1, kFailure};
    const auto failure = DecodeAuthenticationFailure(event.data(), event.size());
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure.value_or(StaleBondRecovery::Failure{}).address, "04:CF:4B:CC:BE:F5");
    EXPECT_EQ(failure.value_or(StaleBondRecovery::Failure{}).type, 1);
    EXPECT_EQ(failure.value_or(StaleBondRecovery::Failure{}).status, kFailure);
    EXPECT_FALSE(DecodeAuthenticationFailure(event.data(), event.size() - 1).has_value());
    auto other_index = event;
    other_index[2] = 1;
    EXPECT_FALSE(DecodeAuthenticationFailure(other_index.data(), other_index.size()).has_value());
}

TEST(StaleBondRecoveryTest, FailedAuthenticationOfBondedConnectedCentralRemovesOnlyItsBond) {
    StaleBondRecovery recovery;
    recovery.Observe({"/org/bluez/hci0/dev_04_CF_4B_CC_BE_F5", "04:CF:4B:CC:BE:F5", 1, true, true});
    recovery.Observe({"/org/bluez/hci0/dev_11_22_33_44_55_66", "11:22:33:44:55:66", 1, true, true});

    EXPECT_EQ(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, Clock::time_point{}),
        "/org/bluez/hci0/dev_04_CF_4B_CC_BE_F5");
}

TEST(StaleBondRecoveryTest, NormalReconnectAndUnbondedOrDisconnectedFailureKeepBonds) {
    StaleBondRecovery recovery;
    const std::string path = "/org/bluez/hci0/dev_04_CF_4B_CC_BE_F5";
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, true});
    EXPECT_FALSE(recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, 0}, Clock::time_point{}));
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, false, true});
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, Clock::time_point{}));
    recovery.Forget(path);
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, false});
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, Clock::time_point{}));
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 2, kFailure}, Clock::time_point{}));
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, Clock::time_point{}));
}

TEST(StaleBondRecoveryTest, DisconnectBeforeAuthFailureStillRemovesPreviouslyConnectedBond) {
    StaleBondRecovery recovery;
    const std::string path = "/org/bluez/hci0/dev_04_CF_4B_CC_BE_F5";
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, true});
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, false});
    EXPECT_EQ(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, Clock::time_point{}),
        path);
}

TEST(StaleBondRecoveryTest, RepeatedFailureRateLimitedPerAddressForSixtySeconds) {
    StaleBondRecovery recovery;
    const std::string path = "/org/bluez/hci0/dev_04_CF_4B_CC_BE_F5";
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, true});
    const auto start = Clock::time_point{};
    EXPECT_EQ(recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, start), path);
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, false});
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, start + kBeforeCooldown));
    recovery.Forget(path);
    recovery.Observe({path, "04:CF:4B:CC:BE:F5", 1, true, true});
    EXPECT_FALSE(
        recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure}, start + kBeforeCooldown));
    EXPECT_EQ(recovery.AuthenticationFailed({"04:CF:4B:CC:BE:F5", 1, kFailure},
                                            start + StaleBondRecovery::kCooldown),
              path);
}

}  // namespace
