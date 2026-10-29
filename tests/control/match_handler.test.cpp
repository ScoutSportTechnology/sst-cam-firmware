// Match event handler: score/clock/banner -> session + overlay (U9, R18/R20).
// Pure — real SessionManager + OverlayController with fake renderer/sink/cleanup.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

#include "app/control/services/handlers/match-state.handler.hpp"
#include "app/control/services/handlers/match.handler.hpp"
#include "app/overlay/ports/overlay-renderer.hpp"
#include "app/overlay/ports/overlay-sink.hpp"
#include "app/overlay/services/overlay_controller/overlay-controller.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/session/models/session-config.hpp"

namespace {

using sst::control::MatchHandler;
using sst::overlay::IOverlayRenderer;
using sst::overlay::IOverlaySink;
using sst::overlay::OverlayController;
using sst::overlay::RenderScene;
using sst::overlay::RgbaImage;

class FakeRenderer final : public IOverlayRenderer {
   public:
    // Param order fixed by the IOverlayRenderer::Render interface contract;
    // cannot reorder in an override.
    auto Render(const RenderScene& /*scene*/,
                std::uint32_t out_width,  // NOLINT(bugprone-easily-swappable-parameters) floor-ok:
                                          // test double; order fixed by IOverlayRenderer::Render,
                                          // cannot reorder in override
                std::uint32_t out_height) -> RgbaImage override {
        RgbaImage img;
        img.width = out_width;
        img.height = out_height;
        img.stride = out_width * 4;
        img.pixels.assign(static_cast<std::size_t>(img.stride) * out_height, 0);
        return img;
    }
};
class FakeSink final : public IOverlaySink {
   public:
    auto PushFrame(const RgbaImage& /*frame*/) -> void override { ++pushes; }
    auto Clear() -> void override { ++clears; }
    int pushes{0};
    int clears{0};
};
class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> bool override { return true; }
    auto StopStreaming() -> void override {}
    auto TeardownWifiDirect() -> void override {}
    auto ResetSelections() -> void override {}
};

// Overlay canvas size for the test controller; any small square works.
constexpr std::uint32_t kOverlayDim = 64;
constexpr std::uint32_t kInitialElapsed = 42;
constexpr auto kRunningSeconds = std::chrono::seconds{5};
constexpr auto kPausedSeconds = std::chrono::seconds{7};
constexpr auto kReconciledSeconds = std::chrono::seconds{8};
constexpr auto kPartialSecond = std::chrono::milliseconds{500};

struct Fixture {
    FakeCleanup cleanup;
    FakeRenderer renderer;
    FakeSink sink;
    std::chrono::steady_clock::time_point now;
    sst::session::SessionManager sm{cleanup, [this] { return now; }};
    OverlayController controller{renderer, sink, sst::common::OutputSize{kOverlayDim, kOverlayDim}};

    Fixture() {
        sst::session::SessionConfig cfg;
        cfg.team_a_id = "home";
        cfg.team_b_id = "away";
        cfg.team_a_name = "Home";
        cfg.team_b_name = "Away";
        cfg.video_output_path = "";  // skip dir creation
        cfg.thumbnail_output_path = "";
        sm.OnConnect();
        sm.OnWifiReady();
        sm.ApplySessionConfig(cfg);
        sm.OnOverlayConfigured();
    }
};

auto ScoreCmd(const std::string& team, std::int32_t delta) -> sst_cam::Command {
    sst_cam::Command cmd;
    cmd.set_correlation_id("s");
    auto* score = cmd.mutable_score_update();
    score->set_team_id(team);
    score->set_delta(delta);
    return cmd;
}

auto MatchCtl(sst_cam::MatchControlAction action, std::uint32_t period = 0) -> sst_cam::Command {
    sst_cam::Command cmd;
    cmd.set_correlation_id("m");
    auto* match_ctl = cmd.mutable_match_control();
    match_ctl->set_action(action);
    match_ctl->set_period(period);
    return cmd;
}

// Final whistle clears the overlay: a finished match has no active scoreboard,
// so the live preview must not keep showing it.
TEST(MatchHandlerTest, AbsoluteStateChangesOnlyPresentFieldsIncludingZeroAndFalse) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });
    fixture.sm.ApplyMatchUpdate([](sst::session::LiveMatch& match) {
        match.score_a = 3;
        match.score_b = 4;
        match.period = 2;
        match.clock_seconds = kInitialElapsed;
        match.clock_running = true;
    });
    sst_cam::Command cmd;
    auto* update = cmd.mutable_set_match_state();
    update->set_score_a(0);
    update->set_elapsed_seconds(0);
    update->set_clock_running(false);
    update->set_status(sst_cam::MATCH_HALF_TIME);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::OK);
    const auto match = fixture.sm.Snapshot().match;
    EXPECT_EQ(match.score_a, 0U);
    EXPECT_EQ(match.score_b, 4U);
    EXPECT_EQ(match.period, 2U);
    EXPECT_EQ(match.clock_seconds, 0U);
    EXPECT_FALSE(match.clock_running);
    EXPECT_EQ(match.segment, sst::session::MatchSegment::kHalfTime);
    EXPECT_EQ(fixture.sink.pushes, 1);

    sst_cam::Command partial;
    partial.mutable_set_match_state()->set_current_period(0);
    partial.mutable_set_match_state()->set_score_b(0);
    EXPECT_EQ(handler.Handle(partial).status(), sst_cam::OK);
    const auto after = fixture.sm.Snapshot().match;
    EXPECT_EQ(after.period, 0U);
    EXPECT_EQ(after.score_b, 0U);
    EXPECT_EQ(after.segment, sst::session::MatchSegment::kHalfTime);
}

TEST(MatchHandlerTest, AbsoluteStateRejectsWhenNoSession) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager{cleanup};
    FakeRenderer renderer;
    FakeSink sink;
    OverlayController controller{renderer, sink, sst::common::OutputSize{kOverlayDim, kOverlayDim}};
    MatchHandler handler(manager, controller, [] { return std::uint64_t{0}; });
    sst_cam::Command cmd;
    cmd.mutable_set_match_state()->set_score_a(1);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::ERROR);
}

TEST(MatchHandlerTest, MonotonicClockAccumulatesOnlyWhileRunningAndSurvivesWallTimeChanges) {
    FakeCleanup cleanup;
    auto now = std::chrono::steady_clock::time_point{};
    sst::session::SessionManager manager{cleanup, [&now] { return now; }};
    ASSERT_TRUE(manager.OnConnect());
    ASSERT_TRUE(manager.OnWifiReady());
    ASSERT_TRUE(manager.ApplySessionConfig({}));
    FakeRenderer renderer;
    FakeSink sink;
    OverlayController controller{renderer, sink, sst::common::OutputSize{kOverlayDim, kOverlayDim}};
    MatchHandler handler(manager, controller, [] { return std::uint64_t{0}; });
    handler.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    now += kRunningSeconds;
    EXPECT_EQ(manager.Snapshot().match.clock_seconds, 5U);
    handler.Handle(MatchCtl(sst_cam::MATCH_CLOCK_PAUSE));
    now += kPausedSeconds;
    EXPECT_EQ(manager.Snapshot().match.clock_seconds, 5U);
    handler.Handle(MatchCtl(sst_cam::MATCH_CLOCK_RESUME));
    now += std::chrono::seconds(3);
    EXPECT_EQ(manager.Snapshot().match.clock_seconds, 8U);
    sst_cam::Command absolute;
    absolute.mutable_set_match_state()->set_elapsed_seconds(kReconciledSeconds.count());
    now += kPartialSecond;
    EXPECT_EQ(handler.Handle(absolute).status(), sst_cam::OK);
    now += std::chrono::seconds(2);
    EXPECT_EQ(manager.Snapshot().match.clock_seconds, 10U);
    handler.TickClock();
    EXPECT_EQ(manager.Snapshot().match.clock_seconds, 10U);
}

TEST(MatchHandlerTest, DisconnectKeepsMatchClockAndIdentity) {
    Fixture fixture;
    const auto original = fixture.sm.Snapshot();
    ASSERT_TRUE(original.config.has_value());
    sst::session::SessionConfig config = original.config.value_or(sst::session::SessionConfig{});
    config.match_uuid = "same-match";
    ASSERT_TRUE(fixture.sm.ApplySessionConfig(config));
    MatchHandler writer(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });
    sst::control::MatchStateHandler reader(fixture.sm, [] { return std::uint64_t{0}; });
    writer.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    fixture.sm.OnDisconnect();
    fixture.now += std::chrono::seconds(3);
    sst_cam::Command get;
    get.mutable_get_match_state();
    const auto state = reader.Handle(get).match_state();
    EXPECT_EQ(state.elapsed_seconds(), 3U);
    EXPECT_TRUE(state.clock_running());
    EXPECT_EQ(state.match_uuid(), "same-match");
}

TEST(MatchHandlerTest, AbsoluteStatusIsReportedWithoutChangingAbsentClockRunning) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });
    sst_cam::Command cmd;
    cmd.mutable_set_match_state()->set_status(sst_cam::MATCH_PAUSED);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::OK);
    sst::control::MatchStateHandler reader(fixture.sm, [] { return std::uint64_t{0}; });
    sst_cam::Command get;
    get.mutable_get_match_state();
    EXPECT_EQ(reader.Handle(get).match_state().status(), sst_cam::MATCH_PAUSED);
    cmd.mutable_set_match_state()->set_status(sst_cam::MATCH_ACTIVE);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::OK);
    EXPECT_EQ(reader.Handle(get).match_state().status(), sst_cam::MATCH_ACTIVE);
    EXPECT_FALSE(fixture.sm.Snapshot().match.clock_running);
    cmd.mutable_set_match_state()->set_status(sst_cam::MATCH_NOT_STARTED);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::OK);
    EXPECT_EQ(reader.Handle(get).match_state().status(), sst_cam::MATCH_NOT_STARTED);
    cmd.mutable_set_match_state()->set_status(sst_cam::MATCH_STATUS_UNKNOWN);
    EXPECT_EQ(handler.Handle(cmd).status(), sst_cam::OK);
    EXPECT_EQ(reader.Handle(get).match_state().status(), sst_cam::MATCH_STATUS_UNKNOWN);
    handler.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    EXPECT_EQ(reader.Handle(get).match_state().status(), sst_cam::MATCH_ACTIVE);
}

TEST(MatchHandlerTest, FinalWhistleClearsOverlay) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });

    handler.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    EXPECT_EQ(fixture.sink.clears, 0);
    EXPECT_EQ(handler.Handle(MatchCtl(sst_cam::MATCH_FINAL_WHISTLE, 1)).status(),
              sst_cam::ResponseStatus::OK);
    EXPECT_EQ(fixture.sink.clears, 1);
}

// After the final whistle the clock is stopped, so a 1 Hz clock tick must NOT
// resurrect the overlay. Regression: TickClock keyed its refresh off
// ApplyMatchUpdate's always-true return, re-pushing the board one tick after the
// final-whistle clear (overlay flickered back ~1s after ending the match).
TEST(MatchHandlerTest, TickAfterFinalWhistleDoesNotResurrectOverlay) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });

    handler.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    handler.Handle(MatchCtl(sst_cam::MATCH_FINAL_WHISTLE, 1));
    const int pushes_after_whistle = fixture.sink.pushes;

    handler.TickClock();
    handler.TickClock();

    EXPECT_EQ(fixture.sink.pushes, pushes_after_whistle);  // clock stopped -> no re-push
}

// ScoreUpdate maps team_id -> home/away and updates live match.
TEST(MatchHandlerTest, ScoreUpdateUpdatesLiveMatch) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });

    EXPECT_EQ(handler.Handle(ScoreCmd("home", 1)).status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(fixture.sm.Snapshot().match.score_a, 1U);
    handler.Handle(ScoreCmd("away", 2));
    EXPECT_EQ(fixture.sm.Snapshot().match.score_b, 2U);

    // Negative delta clamps at zero (any value past the current score of 1).
    handler.Handle(ScoreCmd("home", -5));  // NOLINT(readability-magic-numbers)
    EXPECT_EQ(fixture.sm.Snapshot().match.score_a, 0U);
}

// Unknown team_id is an error.
TEST(MatchHandlerTest, ScoreUpdateUnknownTeamErrors) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });
    auto resp = handler.Handle(ScoreCmd("ghost", 1));
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_FALSE(resp.error_message().empty());
}

// R20: the local clock only advances while running; pause halts it, resume
// continues — the camera never advances the clock without an app event.
// A single linear clock-progression scenario; the cognitive-complexity score is
// inflated by gtest macro expansion and splitting the step-by-step narrative into
// helpers would obscure it — hence the suppression on the next line.
TEST(MatchHandlerTest,  // NOLINT(readability-function-cognitive-complexity)
     ClockIsDisplayOnlyAndGatedByAppEvents) {
    Fixture fixture;
    MatchHandler handler(fixture.sm, fixture.controller, [] { return std::uint64_t{0}; });

    // Before kickoff the clock is not running: ticks do nothing.
    handler.TickClock();
    EXPECT_EQ(fixture.sm.Snapshot().match.clock_seconds, 0U);

    handler.Handle(MatchCtl(sst_cam::MATCH_KICKOFF, 1));
    EXPECT_EQ(fixture.sm.Snapshot().match.period, 1U);
    fixture.now += std::chrono::seconds(2);
    handler.TickClock();
    EXPECT_EQ(fixture.sm.Snapshot().match.clock_seconds, 2U);

    // Pause halts the tick.
    handler.Handle(MatchCtl(sst_cam::MATCH_CLOCK_PAUSE));
    fixture.now += kRunningSeconds;
    handler.TickClock();
    EXPECT_EQ(fixture.sm.Snapshot().match.clock_seconds, 2U);

    // Resume continues it.
    handler.Handle(MatchCtl(sst_cam::MATCH_CLOCK_RESUME));
    fixture.now += std::chrono::seconds(1);
    handler.TickClock();
    EXPECT_EQ(fixture.sm.Snapshot().match.clock_seconds, 3U);

    // Final whistle stops the clock and marks full-time.
    handler.Handle(MatchCtl(sst_cam::MATCH_FINAL_WHISTLE));
    handler.TickClock();
    EXPECT_EQ(fixture.sm.Snapshot().match.clock_seconds, 3U);
    EXPECT_EQ(fixture.sm.Snapshot().match.segment, sst::session::MatchSegment::kFullTime);
}

}  // namespace
