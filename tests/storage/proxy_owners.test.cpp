#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <future>
#include <string>

#include "app/raw_capture/ports/proxy-owners.hpp"

namespace {

constexpr auto kMatch = "01234567-89ab-4cde-8fab-0123456789ab";

class ProxySink final : public sst::raw_capture::IRawCaptureSink {
   public:
    auto Start(const std::string& group, const std::filesystem::path& dir) -> bool override {
        ++starts;
        last_group = group;
        last_dir = dir;
        capturing = can_start;
        return can_start;
    }
    auto Stop() -> bool override {
        ++stops;
        capturing = false;
        return true;
    }
    auto PushCamera(std::uint32_t /*camera_index*/,
                    const sst::capture::Frame& /*frame*/) -> void override {}
    [[nodiscard]] auto IsCapturing() const -> bool override { return capturing; }

    std::atomic<int> starts{0};
    std::atomic<int> stops{0};
    bool can_start{true};
    std::atomic<bool> capturing{false};
    std::string last_group;
    std::filesystem::path last_dir;
};

TEST(ProxyOwnersTest, BothOwnersShareOneCaptureUntilLastStop) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    const auto output = std::filesystem::path("/video/user") / kMatch / "";
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kRecording, kMatch, output));
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming,
                              "ffffffff-ffff-4fff-8fff-ffffffffffff", output));
    EXPECT_EQ(sink.starts, 1);
    EXPECT_EQ(sink.last_group, kMatch);
    EXPECT_EQ(sink.last_dir, output.parent_path());
    owners.Stop(sst::raw_capture::ProxyOwners::Owner::kRecording);
    EXPECT_EQ(sink.stops, 0);
    owners.Stop(sst::raw_capture::ProxyOwners::Owner::kStreaming);
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

TEST(ProxyOwnersTest, StreamFirstAndRepeatedStartsDoNotLeakOrStopEarly) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    const auto output = std::filesystem::path("/video/user") / kMatch / "";
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kRecording, kMatch, output));
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kRecording,
                              "ffffffff-ffff-4fff-8fff-ffffffffffff", output));
    owners.Stop(sst::raw_capture::ProxyOwners::Owner::kStreaming);
    EXPECT_EQ(sink.starts, 1);
    EXPECT_EQ(sink.stops, 0);
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

TEST(ProxyOwnersTest, ConcurrentOwnersAndCleanupCloseExactlyOnce) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    const auto output = std::filesystem::path("/video/user") / kMatch / "";
    auto record = std::async(std::launch::async, [&] {
        return owners.Start(sst::raw_capture::ProxyOwners::Owner::kRecording, kMatch, output);
    });
    auto stream = std::async(std::launch::async, [&] {
        return owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output);
    });
    EXPECT_TRUE(record.get());
    EXPECT_TRUE(stream.get());
    EXPECT_EQ(sink.starts, 1);
    auto stop_record = std::async(
        std::launch::async, [&] { owners.Stop(sst::raw_capture::ProxyOwners::Owner::kRecording); });
    auto stop_stream = std::async(
        std::launch::async, [&] { owners.Stop(sst::raw_capture::ProxyOwners::Owner::kStreaming); });
    stop_record.get();
    stop_stream.get();
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

TEST(ProxyOwnersTest, CleanupClosesUnownedSinkAfterFailedStart) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    sink.capturing = true;
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

TEST(ProxyOwnersTest, StaleStreamOwnerCannotAttachToANewMatch) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    const auto output = std::filesystem::path("/video/user") / kMatch / "";
    constexpr auto kOtherMatch = "ffffffff-ffff-4fff-8fff-ffffffffffff";
    const auto other_output = std::filesystem::path("/video/user") / kOtherMatch / "";
    ASSERT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    EXPECT_FALSE(
        owners.Start(sst::raw_capture::ProxyOwners::Owner::kRecording, kOtherMatch, other_output));
    EXPECT_EQ(sink.starts, 1);
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

TEST(ProxyOwnersTest, InvalidIdentityAndFailedSinkDoNotClaimOwnership) {
    ProxySink sink;
    sst::raw_capture::ProxyOwners owners(sink);
    const auto output = std::filesystem::path("/video/user") / kMatch / "";
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, "../bad", output));
    EXPECT_FALSE(
        owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, "raw__bad__cam0", output));
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch,
                              "/video/user/other/other.mp4"));
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch,
                              "/video/user/other/"));
    EXPECT_EQ(sink.starts, 0);
    sink.can_start = false;
    EXPECT_FALSE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    sink.can_start = true;
    EXPECT_TRUE(owners.Start(sst::raw_capture::ProxyOwners::Owner::kStreaming, kMatch, output));
    owners.StopAll();
    EXPECT_EQ(sink.stops, 1);
}

}  // namespace
