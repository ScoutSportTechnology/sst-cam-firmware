// Session lifecycle SM + in-memory state + disconnect cleanup
// (U6, R11/R12/R13/R15, AE2).
//
// Pure — no hardware. Drives the ordered transitions, asserts output-dir
// creation under a temp root, and asserts the disconnect fan-out via a fake
// ISessionCleanup.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "domain/session/models/session-config.hpp"

namespace fs = std::filesystem;

namespace {

using sst::session::SessionConfig;
using sst::session::SessionManager;
using sst::session::SessionPhase;

constexpr auto kNearDefaultStop = std::chrono::minutes(29);
constexpr auto kDefaultStop = std::chrono::minutes(30);
constexpr std::uint32_t kCustomStopMinutes = 5;

class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> bool override {
        finalize_recording = true;
        if (on_finalize) {
            on_finalize();
        }
        return finalize_ok;
    }
    auto StopStreaming() -> void override { stop_streaming = true; }
    auto TeardownWifiDirect() -> void override { teardown_wifi = true; }
    auto ResetSelections() -> void override { reset_selections = true; }

    std::function<void()> on_finalize;
    bool finalize_ok{true};
    bool finalize_recording{false};
    bool stop_streaming{false};
    bool teardown_wifi{false};
    bool reset_selections{false};
};

// Unique temp root per test to keep filesystem state isolated (no shared dirs).
auto MakeTempRoot() -> fs::path {
    static std::atomic<int> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() / ("sst_session_" + std::to_string(stamp) + "_" +
                                                 std::to_string(counter.fetch_add(1)));
    return root;
}

auto MakeConfig(const fs::path& root) -> SessionConfig {
    // A regulation soccer half is 45 minutes; period_length_seconds is in seconds.
    constexpr std::uint32_t kHalfLengthSeconds = 45U * 60U;
    SessionConfig config;
    config.match_uuid = "match-uuid";
    config.user_uuid = "user-uuid";
    config.sport = "SPORT_SOCCER";
    config.num_periods = 2;
    config.period_length_seconds = kHalfLengthSeconds;
    config.video_output_path = (root / "video/user-uuid/match-uuid/match-uuid.mp4").string();
    config.thumbnail_output_path =
        (root / "thumbnail/user-uuid/match-uuid/match-uuid.jpg").string();
    config.team_a_name = "Home";
    config.team_b_name = "Away";
    return config;
}

// Drive the SM through the full ordered F1 flow.
auto AdvanceToReady(SessionManager& manager, const SessionConfig& cfg) {
    ASSERT_TRUE(manager.OnConnect());
    ASSERT_TRUE(manager.OnWifiReady());
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    ASSERT_TRUE(manager.OnOverlayConfigured());
}

// R12/R13: ordered commands transition Idle -> ... -> Ready and
// PushSessionConfig creates both output directories.
TEST(SessionManagerTest, OrderedTransitionsAndDirCreation) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
    ASSERT_TRUE(manager.OnConnect());
    EXPECT_EQ(manager.Phase(), SessionPhase::kConnected);
    ASSERT_TRUE(manager.OnWifiReady());
    EXPECT_EQ(manager.Phase(), SessionPhase::kWifiReady);
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Phase(), SessionPhase::kConfigured);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);
    ASSERT_TRUE(manager.OnRecordingStart());
    EXPECT_EQ(manager.Phase(), SessionPhase::kRecording);

    EXPECT_TRUE(fs::exists(fs::path{cfg.video_output_path}.parent_path()));
    EXPECT_TRUE(fs::exists(fs::path{cfg.thumbnail_output_path}.parent_path()));

    fs::remove_all(root);
}

// Out-of-order: overlay/config before its prerequisite is rejected (no-op).
TEST(SessionManagerTest, OutOfOrderTransitionsRejected) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    // Overlay before any session config.
    EXPECT_FALSE(manager.OnOverlayConfigured());
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);

    // Session config before the WiFi group is up.
    ASSERT_TRUE(manager.OnConnect());
    EXPECT_FALSE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Phase(), SessionPhase::kConnected);

    // Recording before Ready.
    ASSERT_TRUE(manager.OnWifiReady());
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    EXPECT_FALSE(manager.OnRecordingStart());  // still Configured, not Ready
    EXPECT_EQ(manager.Phase(), SessionPhase::kConfigured);

    fs::remove_all(root);
}

// U8 / R8 / contract §11: session config is accepted only after the WiFi-Direct
// group is up (phase >= kWifiReady). The documented order
// (StartWifiDirect -> PushSessionConfig) succeeds; config before the group is a
// no-op rejection that leaves the phase unchanged.
TEST(SessionManagerTest, SessionConfigGatedOnWifiDirectPerSection11) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    ASSERT_TRUE(manager.OnConnect());

    // Before the WiFi-Direct group: rejected, phase stays Connected.
    EXPECT_FALSE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Phase(), SessionPhase::kConnected);

    // After StartWifiDirect (OnWifiReady): the documented order is accepted.
    ASSERT_TRUE(manager.OnWifiReady());
    EXPECT_TRUE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Phase(), SessionPhase::kConfigured);

    fs::remove_all(root);
}

// U8: the only ordering constraint is the documented §11 one — there is no
// extra firmware-only rule. Overlay-before-session is still rejected (it has its
// own documented prerequisite), and a re-push of config while Configured/Ready
// is accepted (idempotent design-change resend), keeping the phase.
TEST(SessionManagerTest, NoUndocumentedOrderingConstraints) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    // Overlay before any session config: rejected (documented prerequisite).
    ASSERT_TRUE(manager.OnConnect());
    ASSERT_TRUE(manager.OnWifiReady());
    EXPECT_FALSE(manager.OnOverlayConfigured());
    EXPECT_EQ(manager.Phase(), SessionPhase::kWifiReady);

    // Config, then overlay, then a re-push of config while Ready — all accepted,
    // phase held at Ready (no undocumented "config only once" rule).
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    ASSERT_TRUE(manager.OnOverlayConfigured());
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);
    EXPECT_TRUE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);

    fs::remove_all(root);
}

TEST(SessionManagerTest, DifferentMatchConfigRejectedWhileRecording) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.OnRecordingStart());
    ASSERT_EQ(manager.Phase(), SessionPhase::kRecording);

    auto other = cfg;
    other.match_uuid = "other-match";
    EXPECT_FALSE(manager.ApplySessionConfig(other));
    EXPECT_EQ(manager.Phase(), SessionPhase::kRecording);

    fs::remove_all(root);
}

// R11: session memory (config, scores, clock, period) is cleared on session end.
TEST(SessionManagerTest, SameMatchReconfigKeepsActiveRecordingPath) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const auto root = MakeTempRoot();
    const auto config = MakeConfig(root);
    AdvanceToReady(manager, config);
    ASSERT_TRUE(manager.OnRecordingStart());
    auto update = config;
    update.video_output_path = (root / "other.mp4").string();
    update.auto_stop_minutes = kCustomStopMinutes;
    ASSERT_TRUE(manager.ApplySessionConfig(update));
    const auto active = manager.Snapshot().config.value_or(SessionConfig{});
    EXPECT_EQ(active.video_output_path, config.video_output_path);
    EXPECT_EQ(active.auto_stop_minutes, kCustomStopMinutes);
    fs::remove_all(root);
}

TEST(SessionManagerTest, StreamingOnlyReconfigKeepsIdentityAndChangesTimeout) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    auto config = MakeConfig(root);
    AdvanceToReady(manager, config);
    manager.OnStreamingChanged(true);
    manager.OnDisconnect();
    now += std::chrono::minutes(2);
    config.auto_stop_minutes = kCustomStopMinutes;
    ASSERT_TRUE(manager.ApplySessionConfig(config));
    now += std::chrono::minutes(4);
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);
    EXPECT_TRUE(manager.Snapshot().is_streaming);
    now += std::chrono::minutes(1);
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
    fs::remove_all(root);
}

TEST(SessionManagerTest, SessionMemoryClearedOnEnd) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.ApplyMatchUpdate([](sst::session::LiveMatch& match) {
        match.score_a = 2;
        match.score_b = 1;
        match.period = 1;
        match.clock_seconds = 65;
        match.clock_running = true;
    }));

    auto before = manager.Snapshot();
    ASSERT_TRUE(before.config.has_value());
    EXPECT_EQ(before.match.score_a, 2U);

    manager.OnDisconnect();
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);
    manager.EndSession(sst::session::SessionEndReason::kAppStop);

    auto after = manager.Snapshot();
    EXPECT_EQ(after.phase, SessionPhase::kIdle);
    EXPECT_FALSE(after.config.has_value());
    EXPECT_EQ(after.match.score_a, 0U);
    EXPECT_EQ(after.match.score_b, 0U);
    EXPECT_EQ(after.match.period, 0U);
    EXPECT_EQ(after.match.clock_seconds, 0U);
    EXPECT_FALSE(after.match.clock_running);

    // A live-match query after the session ended reflects no session.
    EXPECT_FALSE(
        manager.ApplyMatchUpdate([](sst::session::LiveMatch& match) { match.score_a = 9; }));

    fs::remove_all(root);
}

// #6 stale-overlay fix: a config for a NEW match resets the scoreboard to zero,
// while a same-match re-push preserves the score the app already pushed.
TEST(SessionManagerTest, NewMatchConfigResetsScoreSameMatchKeeps) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.ApplyMatchUpdate([](sst::session::LiveMatch& match) {
        match.score_a = 3;
        match.score_b = 2;
        match.period = 2;
    }));

    // Same match (same match_uuid) re-push: the live score survives.
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    EXPECT_EQ(manager.Snapshot().match.score_a, 3U);
    EXPECT_EQ(manager.Snapshot().match.score_b, 2U);

    // A DIFFERENT match: scoreboard resets to zero so it never inherits the
    // previous match's score.
    auto next = cfg;
    next.match_uuid = "match-uuid-2";
    next.video_output_path = (root / "video/user-uuid/match-uuid-2/match-uuid-2.mp4").string();
    next.thumbnail_output_path =
        (root / "thumbnail/user-uuid/match-uuid-2/match-uuid-2.jpg").string();
    ASSERT_TRUE(manager.ApplySessionConfig(next));

    const auto after = manager.Snapshot();
    EXPECT_EQ(after.match.score_a, 0U);
    EXPECT_EQ(after.match.score_b, 0U);
    EXPECT_EQ(after.match.period, 0U);
    ASSERT_TRUE(after.config.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access) // floor-ok: ASSERT_TRUE guards
    EXPECT_EQ(after.config->match_uuid, "match-uuid-2");

    fs::remove_all(root);
}

TEST(SessionManagerTest, DisconnectWhileRecordingPreservesSessionAndCapture) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.OnRecordingStart());
    EXPECT_EQ(manager.Phase(), SessionPhase::kRecording);

    manager.OnDisconnect();

    EXPECT_FALSE(cleanup.finalize_recording);
    EXPECT_FALSE(cleanup.stop_streaming);
    EXPECT_FALSE(cleanup.teardown_wifi);
    EXPECT_FALSE(cleanup.reset_selections);
    EXPECT_EQ(manager.Phase(), SessionPhase::kRecording);
    const auto state = manager.Snapshot();
    ASSERT_TRUE(state.config.has_value());
    EXPECT_EQ(state.config.value_or(SessionConfig{}).match_uuid, "match-uuid");

    fs::remove_all(root);
}

// Disconnect from Idle is a no-op (no cleanup fan-out).
TEST(SessionManagerTest, DisconnectFromIdleIsNoop) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    manager.OnDisconnect();
    EXPECT_FALSE(cleanup.finalize_recording);
    EXPECT_FALSE(cleanup.stop_streaming);
    EXPECT_FALSE(cleanup.teardown_wifi);
    EXPECT_FALSE(cleanup.reset_selections);
}

// A fake whose FinalizeRecording throws, so the exception-safety test can prove
// the disconnect fan-out runs the remaining steps despite an earlier throw.
// Hoisted to namespace scope so its virtual overrides don't inflate the test
// body's cognitive complexity.
class ThrowingFinalizeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> bool override {
        finalize_attempted = true;
        throw std::runtime_error("recorder blew up on finalize");
    }
    auto StopStreaming() -> void override { stop_streaming = true; }
    auto TeardownWifiDirect() -> void override { teardown_wifi = true; }
    auto ResetSelections() -> void override { reset_selections = true; }

    bool finalize_attempted{false};
    bool stop_streaming{false};
    bool teardown_wifi{false};
    bool reset_selections{false};
};

// A cleanup step that throws must not abort the disconnect: the remaining steps
// still run and the session is still reset to Idle (exception-safe fan-out).
TEST(SessionManagerTest, DisconnectCleanupIsExceptionSafe) {
    ThrowingFinalizeCleanup cleanup;
    SessionManager manager(cleanup);
    const fs::path root = MakeTempRoot();
    const auto cfg = MakeConfig(root);

    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.OnRecordingStart());

    manager.OnDisconnect();
    EXPECT_NO_THROW(manager.EndSession(sst::session::SessionEndReason::kAppStop));

    EXPECT_TRUE(cleanup.finalize_attempted);
    EXPECT_TRUE(cleanup.stop_streaming);              // ran despite the earlier throw
    EXPECT_TRUE(cleanup.teardown_wifi);               // ran despite the earlier throw
    EXPECT_TRUE(cleanup.reset_selections);            // ran despite the earlier throw
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);  // session still reset

    fs::remove_all(root);
}

TEST(SessionManagerTest, ReconnectCancelsDefaultDeadlineAndKeepsIdentity) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    AdvanceToReady(manager, MakeConfig(root));
    ASSERT_TRUE(manager.OnRecordingStart());
    manager.OnDisconnect();
    now += kNearDefaultStop;
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kRecording);
    ASSERT_TRUE(manager.OnConnect());
    now += std::chrono::minutes(2);
    manager.Tick();
    ASSERT_TRUE(manager.Snapshot().config.has_value());
    EXPECT_EQ(manager.Snapshot().config.value_or(SessionConfig{}).match_uuid, "match-uuid");
    EXPECT_FALSE(cleanup.finalize_recording);
    fs::remove_all(root);
}

TEST(SessionManagerTest, CustomTimeoutRearmsFromConfigAndFinalizesOnce) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    auto config = MakeConfig(root);
    config.auto_stop_minutes = kCustomStopMinutes;
    AdvanceToReady(manager, config);
    manager.OnDisconnect();
    now += std::chrono::minutes(4);
    manager.Tick();
    EXPECT_FALSE(cleanup.teardown_wifi);
    now += std::chrono::minutes(1);
    manager.Tick();
    manager.Tick();
    EXPECT_TRUE(cleanup.teardown_wifi);
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
    ASSERT_TRUE(manager.Snapshot().last_session.has_value());
    EXPECT_EQ(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).reason,
              sst::session::SessionEndReason::kAutoStop);
    fs::remove_all(root);
}

TEST(SessionManagerTest, StreamingOnlySurvivesDisconnectThenAutoStops) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    AdvanceToReady(manager, MakeConfig(root));
    manager.OnStreamingChanged(true);
    manager.OnDisconnect();
    EXPECT_TRUE(manager.Snapshot().is_streaming);
    EXPECT_FALSE(cleanup.stop_streaming);
    now += kDefaultStop;
    manager.Tick();
    EXPECT_TRUE(cleanup.stop_streaming);
    EXPECT_FALSE(cleanup.finalize_recording);
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
    EXPECT_EQ(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).reason,
              sst::session::SessionEndReason::kAutoStop);
    fs::remove_all(root);
}

TEST(SessionManagerTest, RepeatedDisconnectDoesNotExtendDeadline) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    ASSERT_TRUE(manager.OnConnect());
    ASSERT_TRUE(manager.OnWifiReady());
    manager.OnDisconnect();
    now += kNearDefaultStop;
    manager.OnDisconnect();
    now += std::chrono::minutes(1);
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
}

TEST(SessionManagerTest, WifiOnlyDisconnectedSessionTimesOutAtDefault) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    ASSERT_TRUE(manager.OnConnect());
    ASSERT_TRUE(manager.OnWifiReady());
    manager.OnDisconnect();
    now += kDefaultStop;
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kIdle);
    EXPECT_TRUE(cleanup.teardown_wifi);
    EXPECT_FALSE(manager.Snapshot().last_session.has_value());
}

TEST(SessionManagerTest, StaleExpiryCannotFinalizeReconnectedSession) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    AdvanceToReady(manager, MakeConfig(root));
    manager.OnDisconnect();
    now += kDefaultStop;
    ASSERT_TRUE(manager.OnConnect());
    manager.Tick();
    EXPECT_EQ(manager.Phase(), SessionPhase::kReady);
    EXPECT_FALSE(cleanup.teardown_wifi);
    fs::remove_all(root);
}

TEST(SessionManagerTest, BootOrphanReportsRebootAndInvalidFile) {
    FakeCleanup cleanup;
    const auto root = MakeTempRoot();
    auto cfg = MakeConfig(root);
    cfg.video_output_path = (root / "video/user-uuid/match-uuid/clip-001.mp4").string();
    {
        SessionManager manager(cleanup, std::chrono::steady_clock::now, root / "video");
        AdvanceToReady(manager, cfg);
        ASSERT_TRUE(manager.OnRecordingStart());
    }
    SessionManager rebooted(cleanup, std::chrono::steady_clock::now, root / "video");
    ASSERT_TRUE(rebooted.Snapshot().last_session.has_value());
    EXPECT_EQ(rebooted.Snapshot().last_session.value_or(sst::session::LastSession{}).match_uuid,
              "match-uuid");
    EXPECT_EQ(rebooted.Snapshot().last_session.value_or(sst::session::LastSession{}).reason,
              sst::session::SessionEndReason::kReboot);
    EXPECT_FALSE(rebooted.Snapshot().last_session.value_or(sst::session::LastSession{}).file_valid);
    fs::remove_all(root);
}

TEST(SessionManagerTest, FailedFinalizeKeepsOrphanMarkerForBootRecovery) {
    FakeCleanup cleanup;
    cleanup.finalize_ok = false;
    const auto root = MakeTempRoot();
    const auto cfg = MakeConfig(root);
    SessionManager manager(cleanup, std::chrono::steady_clock::now, root / "video");
    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.OnRecordingStart());
    manager.OnDisconnect();
    EXPECT_TRUE(manager.EndSession(sst::session::SessionEndReason::kAppStop));
    EXPECT_FALSE(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).file_valid);
    SessionManager rebooted(cleanup, std::chrono::steady_clock::now, root / "video");
    EXPECT_EQ(rebooted.Snapshot().last_session.value_or(sst::session::LastSession{}).reason,
              sst::session::SessionEndReason::kReboot);
    fs::remove_all(root);
}

TEST(SessionManagerTest, ExplicitStopWithFailedEosKeepsOrphanMarker) {
    FakeCleanup cleanup;
    const auto root = MakeTempRoot();
    const auto cfg = MakeConfig(root);
    const auto marker = fs::path(cfg.video_output_path).replace_extension(".session-active");
    SessionManager manager(cleanup, std::chrono::steady_clock::now, root / "video");
    AdvanceToReady(manager, cfg);
    ASSERT_TRUE(manager.OnRecordingStart());
    ASSERT_TRUE(fs::exists(marker));
    ASSERT_TRUE(manager.OnRecordingStop(false));
    EXPECT_TRUE(fs::exists(marker));
    EXPECT_FALSE(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).file_valid);
    fs::remove_all(root);
}

TEST(SessionManagerTest, CameraFailureFinalizesWithReason) {
    FakeCleanup cleanup;
    SessionManager manager(cleanup);
    const auto root = MakeTempRoot();
    AdvanceToReady(manager, MakeConfig(root));
    ASSERT_TRUE(manager.OnRecordingStart());
    EXPECT_TRUE(manager.EndSession(sst::session::SessionEndReason::kCameraFailure));
    EXPECT_TRUE(cleanup.finalize_recording);
    EXPECT_EQ(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).reason,
              sst::session::SessionEndReason::kCameraFailure);
    fs::remove_all(root);
}

TEST(SessionManagerTest, FinalizingIsVisibleAndPreservesClockAndValidity) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    SessionManager manager(cleanup, [&] { return now; });
    const auto root = MakeTempRoot();
    AdvanceToReady(manager, MakeConfig(root));
    ASSERT_TRUE(manager.OnRecordingStart());
    ASSERT_TRUE(
        manager.ApplyMatchUpdate([](sst::session::LiveMatch& match) { match.clock_seconds = 47; }));
    cleanup.on_finalize = [&] { EXPECT_EQ(manager.Phase(), SessionPhase::kFinalizing); };
    EXPECT_TRUE(manager.EndSession(sst::session::SessionEndReason::kAppStop));
    EXPECT_FALSE(manager.EndSession(sst::session::SessionEndReason::kAppStop));
    ASSERT_TRUE(manager.Snapshot().last_session.has_value());
    EXPECT_EQ(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).match_uuid,
              "match-uuid");
    EXPECT_EQ(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).clock_seconds,
              47U);
    EXPECT_TRUE(manager.Snapshot().last_session.value_or(sst::session::LastSession{}).file_valid);
    fs::remove_all(root);
}

}  // namespace
