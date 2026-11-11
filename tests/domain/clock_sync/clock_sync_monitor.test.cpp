#include "domain/clock_sync/clock_sync_monitor.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace sst::domain::clock_sync {

class FakeClockSyncSource final : public IClockSyncSource {
   public:
    explicit FakeClockSyncSource(std::vector<bool> states) : states_(std::move(states)) {}

    [[nodiscard]] auto IsSynced() const -> bool override {
        if (index_ < states_.size()) {
            return states_[index_++];
        }
        // Return last state if we run out
        return states_.empty() ? false : states_.back();
    }

   private:
    std::vector<bool> states_;
    mutable size_t index_ = 0;
};

class ClockSyncMonitorTest : public ::testing::Test {
   protected:
    void SetUp() override { logs_.clear(); }

    void LogCallback(const std::string& msg) { logs_.push_back(msg); }

    std::vector<std::string> logs_;
};

TEST_F(ClockSyncMonitorTest, StartsUnsyncedLogsNo) {
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{false});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();

    ASSERT_EQ(logs_.size(), 1);
    EXPECT_EQ(logs_[0], "clock: synced=no");
}

TEST_F(ClockSyncMonitorTest, StartsSyncedLogsYes) {
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{true});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();

    ASSERT_EQ(logs_.size(), 1);
    EXPECT_EQ(logs_[0], "clock: synced=yes");
}

TEST_F(ClockSyncMonitorTest, TransitionsFromUnsyncedToSyncedLogsJump) {
    // Start unsynced, then synced
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{false, true});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();  // First call: unsynced
    monitor.CheckAndLog();  // Second call: now synced

    ASSERT_EQ(logs_.size(), 2);
    EXPECT_EQ(logs_[0], "clock: synced=no");
    EXPECT_EQ(logs_[1], "clock: synced=yes (wall-clock jump detected)");
}

TEST_F(ClockSyncMonitorTest, TransitionsFromSyncedToUnsyncedLogsNo) {
    // Start synced, then unsynced
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{true, false});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();  // First call: synced
    monitor.CheckAndLog();  // Second call: now unsynced

    ASSERT_EQ(logs_.size(), 2);
    EXPECT_EQ(logs_[0], "clock: synced=yes");
    EXPECT_EQ(logs_[1], "clock: synced=no");
}

TEST_F(ClockSyncMonitorTest, AlreadySyncedAtStartNoDuplicateLog) {
    // Stays synced across multiple checks
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{true, true, true});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();  // First call: synced
    monitor.CheckAndLog();  // Second call: still synced
    monitor.CheckAndLog();  // Third call: still synced

    ASSERT_EQ(logs_.size(), 1);
    EXPECT_EQ(logs_[0], "clock: synced=yes");
}

TEST_F(ClockSyncMonitorTest, AlreadyUnsyncedAtStartNoDuplicateLog) {
    // Stays unsynced across multiple checks
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{false, false, false});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();  // First call: unsynced
    monitor.CheckAndLog();  // Second call: still unsynced
    monitor.CheckAndLog();  // Third call: still unsynced

    ASSERT_EQ(logs_.size(), 1);
    EXPECT_EQ(logs_[0], "clock: synced=no");
}

TEST_F(ClockSyncMonitorTest, IsSyncedReturnsCurrentState) {
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{false, true});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    // Before any CheckAndLog call, IsSynced returns false (initial state)
    EXPECT_FALSE(monitor.IsSynced());

    monitor.CheckAndLog();  // First call: unsynced
    EXPECT_FALSE(monitor.IsSynced());

    monitor.CheckAndLog();  // Second call: synced
    EXPECT_TRUE(monitor.IsSynced());
}

TEST_F(ClockSyncMonitorTest, HardwareBootSequenceUnsyncedThenSynced) {
    // Reproduces the hardware sequence observed:
    // 1. At boot: clock is NOT synced (1969 timestamp, NTPSynchronized=no)
    // 2. NTP sync occurs: clock becomes synced (NTPSynchronized=yes)
    // Expected: exactly one "clock: synced=no" on first check, then one transition to synced
    auto source = std::make_unique<FakeClockSyncSource>(std::vector<bool>{false, true, true, true});
    ClockSyncMonitor monitor(std::move(source),
                             [this](const std::string& msg) { LogCallback(msg); });

    monitor.CheckAndLog();  // First call at boot: unsynced
    monitor.CheckAndLog();  // Second call: now synced
    monitor.CheckAndLog();  // Third call: still synced
    monitor.CheckAndLog();  // Fourth call: still synced

    ASSERT_EQ(logs_.size(), 2);
    EXPECT_EQ(logs_[0], "clock: synced=no");
    EXPECT_EQ(logs_[1], "clock: synced=yes (wall-clock jump detected)");
}

}  // namespace sst::domain::clock_sync