// Session lifecycle SM + in-memory state + disconnect cleanup
// (U6, R11/R12/R13/R15, AE2).
//
// Pure — no hardware. Drives the ordered transitions, asserts output-dir
// creation under a temp root, and asserts the disconnect fan-out via a fake
// ISessionCleanup.

#include "app/session/services/session_manager/session-manager.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "app/session/ports/session-cleanup.hpp"
#include "domain/session/models/session-config.hpp"

namespace fs = std::filesystem;

namespace {

using sst::session::SessionConfig;
using sst::session::SessionManager;
using sst::session::SessionPhase;

class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> void override { finalize_recording = true; }
    auto StopStreaming() -> void override { stop_streaming = true; }
    auto TeardownWifiDirect() -> void override { teardown_wifi = true; }

    bool finalize_recording{false};
    bool stop_streaming{false};
    bool teardown_wifi{false};
};

// Unique temp root per test to keep filesystem state isolated (no shared dirs).
auto MakeTempRoot() -> fs::path {
    static std::atomic<int> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() /
                    ("sst_session_" + std::to_string(stamp) + "_" +
                     std::to_string(counter.fetch_add(1)));
    return root;
}

auto MakeConfig(const fs::path& root) -> SessionConfig {
    SessionConfig c;
    c.match_uuid = "match-uuid";
    c.user_uuid = "user-uuid";
    c.sport = "SPORT_SOCCER";
    c.num_periods = 2;
    c.period_length_seconds = 2700;
    c.video_output_path = (root / "video/user-uuid/match-uuid/match-uuid.mp4").string();
    c.thumbnail_output_path = (root / "thumbnail/user-uuid/match-uuid/match-uuid.jpg").string();
    c.team_a_name = "Home";
    c.team_b_name = "Away";
    return c;
}

// Drive the SM through the full ordered F1 flow.
auto AdvanceToReady(SessionManager& sm, const SessionConfig& cfg) {
    ASSERT_TRUE(sm.OnConnect());
    ASSERT_TRUE(sm.OnWifiReady());
    ASSERT_TRUE(sm.ApplySessionConfig(cfg));
    ASSERT_TRUE(sm.OnOverlayConfigured());
}

// R12/R13: ordered commands transition Idle -> ... -> Ready and
// PushSessionConfig creates both output directories.
TEST(SessionManagerTest, OrderedTransitionsAndDirCreation) {
    FakeCleanup cleanup;
    SessionManager sm(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    EXPECT_EQ(sm.Phase(), SessionPhase::kIdle);
    ASSERT_TRUE(sm.OnConnect());
    EXPECT_EQ(sm.Phase(), SessionPhase::kConnected);
    ASSERT_TRUE(sm.OnWifiReady());
    EXPECT_EQ(sm.Phase(), SessionPhase::kWifiReady);
    ASSERT_TRUE(sm.ApplySessionConfig(cfg));
    EXPECT_EQ(sm.Phase(), SessionPhase::kConfigured);
    ASSERT_TRUE(sm.OnOverlayConfigured());
    EXPECT_EQ(sm.Phase(), SessionPhase::kReady);
    ASSERT_TRUE(sm.OnRecordingStart());
    EXPECT_EQ(sm.Phase(), SessionPhase::kRecording);

    EXPECT_TRUE(fs::exists(fs::path{cfg.video_output_path}.parent_path()));
    EXPECT_TRUE(fs::exists(fs::path{cfg.thumbnail_output_path}.parent_path()));

    fs::remove_all(root);
}

// Out-of-order: overlay/config before its prerequisite is rejected (no-op).
TEST(SessionManagerTest, OutOfOrderTransitionsRejected) {
    FakeCleanup cleanup;
    SessionManager sm(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    // Overlay before any session config.
    EXPECT_FALSE(sm.OnOverlayConfigured());
    EXPECT_EQ(sm.Phase(), SessionPhase::kIdle);

    // Session config before the WiFi group is up.
    ASSERT_TRUE(sm.OnConnect());
    EXPECT_FALSE(sm.ApplySessionConfig(cfg));
    EXPECT_EQ(sm.Phase(), SessionPhase::kConnected);

    // Recording before Ready.
    ASSERT_TRUE(sm.OnWifiReady());
    ASSERT_TRUE(sm.ApplySessionConfig(cfg));
    EXPECT_FALSE(sm.OnRecordingStart());  // still Configured, not Ready
    EXPECT_EQ(sm.Phase(), SessionPhase::kConfigured);

    fs::remove_all(root);
}

// R11: session memory (config, scores, clock, period) is cleared on session end.
TEST(SessionManagerTest, SessionMemoryClearedOnEnd) {
    FakeCleanup cleanup;
    SessionManager sm(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(sm, cfg);
    ASSERT_TRUE(sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.score_a = 2;
        m.score_b = 1;
        m.period = 1;
        m.clock_seconds = 65;
        m.clock_running = true;
    }));

    auto before = sm.Snapshot();
    ASSERT_TRUE(before.config.has_value());
    EXPECT_EQ(before.match.score_a, 2U);

    sm.OnDisconnect();

    auto after = sm.Snapshot();
    EXPECT_EQ(after.phase, SessionPhase::kIdle);
    EXPECT_FALSE(after.config.has_value());
    EXPECT_EQ(after.match.score_a, 0U);
    EXPECT_EQ(after.match.score_b, 0U);
    EXPECT_EQ(after.match.period, 0U);
    EXPECT_EQ(after.match.clock_seconds, 0U);
    EXPECT_FALSE(after.match.clock_running);

    // A live-match query after the session ended reflects no session.
    EXPECT_FALSE(sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) { m.score_a = 9; }));

    fs::remove_all(root);
}

// AE2 / R15: from Recording, a disconnect fans out finalize + stop-stream +
// teardown-WFD (all of them, order-independent) and clears the session.
TEST(SessionManagerTest, DisconnectWhileRecordingFansOutCleanup) {
    FakeCleanup cleanup;
    SessionManager sm(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(sm, cfg);
    ASSERT_TRUE(sm.OnRecordingStart());
    EXPECT_EQ(sm.Phase(), SessionPhase::kRecording);

    sm.OnDisconnect();

    EXPECT_TRUE(cleanup.finalize_recording);
    EXPECT_TRUE(cleanup.stop_streaming);
    EXPECT_TRUE(cleanup.teardown_wifi);
    EXPECT_EQ(sm.Phase(), SessionPhase::kIdle);

    fs::remove_all(root);
}

// Disconnect from Idle is a no-op (no cleanup fan-out).
TEST(SessionManagerTest, DisconnectFromIdleIsNoop) {
    FakeCleanup cleanup;
    SessionManager sm(cleanup);
    sm.OnDisconnect();
    EXPECT_FALSE(cleanup.finalize_recording);
    EXPECT_FALSE(cleanup.stop_streaming);
    EXPECT_FALSE(cleanup.teardown_wifi);
}

// A cleanup step that throws must not abort the disconnect: the remaining steps
// still run and the session is still reset to Idle (exception-safe fan-out).
TEST(SessionManagerTest, DisconnectCleanupIsExceptionSafe) {
    class ThrowingFinalizeCleanup final : public sst::session::ISessionCleanup {
       public:
        auto FinalizeRecording() -> void override {
            finalize_attempted = true;
            throw std::runtime_error("recorder blew up on finalize");
        }
        auto StopStreaming() -> void override { stop_streaming = true; }
        auto TeardownWifiDirect() -> void override { teardown_wifi = true; }

        bool finalize_attempted{false};
        bool stop_streaming{false};
        bool teardown_wifi{false};
    };

    ThrowingFinalizeCleanup cleanup;
    SessionManager sm(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(sm, cfg);
    ASSERT_TRUE(sm.OnRecordingStart());

    EXPECT_NO_THROW(sm.OnDisconnect());

    EXPECT_TRUE(cleanup.finalize_attempted);
    EXPECT_TRUE(cleanup.stop_streaming);   // ran despite the earlier throw
    EXPECT_TRUE(cleanup.teardown_wifi);    // ran despite the earlier throw
    EXPECT_EQ(sm.Phase(), SessionPhase::kIdle);  // session still reset

    fs::remove_all(root);
}

}  // namespace
