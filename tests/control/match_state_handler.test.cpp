// GetMatchState handler: live-match snapshot -> MatchState response (U6, R6).
// Pure — real SessionManager with a fake cleanup; no transport, no hardware.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

#include "app/control/services/dispatcher/command-dispatcher.hpp"
#include "app/control/services/handlers/match-state.handler.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/session/models/session-config.hpp"

namespace {

using sst::control::MatchStateHandler;

class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> void override {}
    auto StopStreaming() -> void override {}
    auto TeardownWifiDirect() -> void override {}
};

// A session driven up to Configured with the given period length, ready to take
// match events.
struct Fixture {
    FakeCleanup cleanup;
    sst::session::SessionManager sm{cleanup};

    explicit Fixture(std::int32_t period_length = 600) {
        sst::session::SessionConfig cfg;
        cfg.team_a_id = "home";
        cfg.team_b_id = "away";
        cfg.period_length_seconds = period_length;
        cfg.video_output_path = "";
        cfg.thumbnail_output_path = "";
        sm.OnConnect();
        sm.OnWifiReady();
        sm.ApplySessionConfig(cfg);
        sm.OnOverlayConfigured();
    }
};

auto GetMatchStateCmd() -> sst_cam::Command {
    sst_cam::Command c;
    c.set_correlation_id("ms-1");
    c.mutable_get_match_state();
    return c;
}

// Happy path: a populated live match maps onto a MatchState payload (OK status,
// echoed correlation_id, scores/period/teams from the snapshot).
TEST(MatchStateHandlerTest, ReportsLiveMatchSnapshot) {
    Fixture f(/*period_length=*/600);
    f.sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.score_a = 2;
        m.score_b = 1;
        m.period = 1;
        m.clock_seconds = 90;
        m.clock_running = true;
        m.segment = sst::session::MatchSegment::kInPlay;
    });

    MatchStateHandler handler(f.sm, [] { return std::uint64_t{12345}; });
    auto resp = handler.Handle(GetMatchStateCmd());

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kMatchState);
    const auto& ms = resp.match_state();
    EXPECT_EQ(ms.status(), sst_cam::MATCH_ACTIVE);
    EXPECT_EQ(ms.current_period(), 1U);
    EXPECT_EQ(ms.score_a(), 2U);
    EXPECT_EQ(ms.score_b(), 1U);
    EXPECT_EQ(ms.team_a_id(), "home");
    EXPECT_EQ(ms.team_b_id(), "away");
    EXPECT_EQ(ms.time_remaining_s(), 510U);  // 600 - 90
    EXPECT_EQ(ms.updated_at(), 12345U);
}

// A paused clock reports MATCH_PAUSED; a full whistle reports MATCH_FINISHED.
TEST(MatchStateHandlerTest, StatusReflectsClockAndSegment) {
    Fixture f;
    MatchStateHandler handler(f.sm, [] { return std::uint64_t{0}; });

    f.sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.period = 1;
        m.clock_running = false;
        m.segment = sst::session::MatchSegment::kInPlay;
    });
    EXPECT_EQ(handler.Handle(GetMatchStateCmd()).match_state().status(), sst_cam::MATCH_PAUSED);

    f.sm.ApplyMatchUpdate(
        [](sst::session::LiveMatch& m) { m.segment = sst::session::MatchSegment::kFullTime; });
    EXPECT_EQ(handler.Handle(GetMatchStateCmd()).match_state().status(), sst_cam::MATCH_FINISHED);
}

// Half-time segment maps onto MATCH_HALF_TIME regardless of the clock state.
TEST(MatchStateHandlerTest, HalfTimeSegmentReportsHalfTime) {
    Fixture f;
    f.sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.period = 1;
        m.clock_running = false;
        m.segment = sst::session::MatchSegment::kHalfTime;
    });
    MatchStateHandler handler(f.sm, [] { return std::uint64_t{0}; });
    EXPECT_EQ(handler.Handle(GetMatchStateCmd()).match_state().status(), sst_cam::MATCH_HALF_TIME);
}

// In-play but before kickoff (period == 0) is not started yet — even though a
// session is configured, no period has begun.
TEST(MatchStateHandlerTest, InPlayBeforeKickoffReportsNotStarted) {
    Fixture f;
    f.sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.period = 0;  // no period started
        m.clock_running = false;
        m.segment = sst::session::MatchSegment::kInPlay;
    });
    MatchStateHandler handler(f.sm, [] { return std::uint64_t{0}; });
    EXPECT_EQ(handler.Handle(GetMatchStateCmd()).match_state().status(),
              sst_cam::MATCH_NOT_STARTED);
}

// time_remaining_s clamps at zero when the locally-ticked clock has run past the
// configured period length (never wraps/underflows).
TEST(MatchStateHandlerTest, TimeRemainingClampsAtZeroWhenElapsedExceedsLength) {
    Fixture f(/*period_length=*/600);
    f.sm.ApplyMatchUpdate([](sst::session::LiveMatch& m) {
        m.period = 1;
        m.clock_seconds = 650;  // past the 600s period length
        m.clock_running = true;
        m.segment = sst::session::MatchSegment::kInPlay;
    });
    MatchStateHandler handler(f.sm, [] { return std::uint64_t{0}; });
    EXPECT_EQ(handler.Handle(GetMatchStateCmd()).match_state().time_remaining_s(), 0U);
}

// No active session (Idle) -> MATCH_NOT_STARTED, still OK, no crash.
TEST(MatchStateHandlerTest, NoSessionReportsNotStarted) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm{cleanup};  // never configured -> Idle
    MatchStateHandler handler(sm, [] { return std::uint64_t{0}; });

    auto resp = handler.Handle(GetMatchStateCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.match_state().status(), sst_cam::MATCH_NOT_STARTED);
    EXPECT_EQ(resp.match_state().time_remaining_s(), 0U);
}

// Dispatcher parity: get_match_state no longer falls through to UNSUPPORTED once
// the handler is registered.
TEST(MatchStateHandlerTest, DispatcherRoutesGetMatchState) {
    Fixture f;
    sst::control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<MatchStateHandler>(f.sm, [] { return std::uint64_t{0}; }));

    auto resp = dispatcher.Dispatch(GetMatchStateCmd());
    EXPECT_NE(resp.status(), sst_cam::ResponseStatus::UNSUPPORTED);
    EXPECT_EQ(resp.correlation_id(), "ms-1");
    EXPECT_EQ(resp.payload_case(), sst_cam::CommandResponse::kMatchState);
}

}  // namespace
