#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/db/services/db_manager/db-manager.hpp"
#include "app/match/services/match_service/match-service.hpp"
#include "app/storage/ports/recording-service.hpp"
#include "domain/storage/models/event-clip-window.hpp"
#include "domain/storage/models/recording-state.hpp"

namespace {

using sst::match::MatchService;
using sst::match::RecordEventRequest;
using sst::match::SetScoreRequest;
using sst::match::StartMatchRequest;
using sst::match::TeamSide;
using sst::storage::EventClipWindow;
using sst::storage::IRecordingService;
using sst::storage::RecordingState;
using sst::storage::StartFullGameResult;
using sst::storage::StopFullGameResult;
using sst::storage::TriggerEventClipResult;

constexpr int64_t kDefaultUserId = 1;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

// ── Test double for the storage module's IRecordingService ────────────
//
// MatchService delegates lifecycle (StartFullGame / StopFullGame /
// TriggerEventClip) to IRecordingService. The storage module's real
// implementation needs NVENC; in container tests we substitute this fake so
// MatchService's state machine + DB orchestration is exercised in isolation.
class FakeRecordingService final : public IRecordingService {
   public:
    auto StartFullGame(const std::string& match_id) -> StartFullGameResult override {
        ++start_calls;
        last_match_id = match_id;
        if (!accept_start) {
            return {.success = false, .recording_id = {}};
        }
        last_recording_id = "rec-" + match_id;
        state_ = RecordingState::kRecording;
        return {.success = true, .recording_id = last_recording_id};
    }

    auto Pause() -> bool override {
        ++pause_calls;
        if (state_ != RecordingState::kRecording) return false;
        state_ = RecordingState::kPaused;
        return true;
    }

    auto Resume() -> bool override {
        ++resume_calls;
        if (state_ != RecordingState::kPaused) return false;
        state_ = RecordingState::kRecording;
        return true;
    }

    auto StopFullGame() -> StopFullGameResult override {
        ++stop_calls;
        if (state_ == RecordingState::kIdle) {
            return {.success = false, .merged_path = {}};
        }
        state_ = RecordingState::kIdle;
        return {.success = true,
                .merged_path = "/tmp/sst-test/" + last_match_id + "/full_game.mp4"};
    }

    auto TriggerEventClip(const std::string& match_event_id, const EventClipWindow& window)
        -> TriggerEventClipResult override {
        ++trigger_calls;
        last_event_id = match_event_id;
        last_window = window;
        if (!accept_trigger) {
            return {};
        }
        TriggerEventClipResult res;
        res.success = true;
        res.event_clip_id = "clip-" + match_event_id;
        res.recording_id = "rec-clip-" + match_event_id;
        res.file_path = "/tmp/sst-test/clip_" + match_event_id + ".mp4";
        return res;
    }

    [[nodiscard]] auto CurrentState() const -> RecordingState override { return state_; }

    int start_calls{0};
    int pause_calls{0};
    int resume_calls{0};
    int stop_calls{0};
    int trigger_calls{0};
    bool accept_start{true};
    bool accept_trigger{true};
    std::string last_match_id;
    std::string last_recording_id;
    std::string last_event_id;
    EventClipWindow last_window{};

   private:
    RecordingState state_{RecordingState::kIdle};
};

// ── Fixture ───────────────────────────────────────────────────────────────

class MatchServiceTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/match/data/test_" + suffix + ".db"))
                       .string();
        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });

        // Seed a default user (FK target for camera_config + match.user_id).
        ASSERT_TRUE(mgr_->users().create("test").success);
        // Seed default camera_config so MatchService can read event_clip_pre/post.
        sst::db::CameraConfig cam;
        cam.user_id = kDefaultUserId;
        ASSERT_TRUE(mgr_->cameras().saveConfig(cam).success);

        // Soccer + a couple of event kinds.
        soccer_id_ = mgr_->sports().save({.code = "soccer", .name = "Soccer", .periods = 2}).data.id;
        ASSERT_GT(soccer_id_, 0);
        ASSERT_TRUE(mgr_->sports()
                        .saveEventKind({.sport_id = soccer_id_, .code = "goal", .name = "Goal"})
                        .success);
        ASSERT_TRUE(mgr_->sports()
                        .saveEventKind({.sport_id = soccer_id_, .code = "yellow_card",
                                        .name = "Yellow Card"})
                        .success);

        // Two soccer teams.
        team_a_id_ = mgr_->teams()
                         .save({.sport_id = soccer_id_, .name = "Alpha", .short_name = "A"})
                         .data.id;
        team_b_id_ = mgr_->teams()
                         .save({.sport_id = soccer_id_, .name = "Bravo", .short_name = "B"})
                         .data.id;
        ASSERT_GT(team_a_id_, 0);
        ASSERT_GT(team_b_id_, 0);

        recording_ = std::make_unique<FakeRecordingService>();
        service_ = std::make_unique<MatchService>(mgr_->matches(), mgr_->sports(),
                                                  mgr_->cameras(), *recording_, kDefaultUserId);
    }

    auto TearDown() -> void override {
        service_.reset();
        recording_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    std::string db_path_;
    std::unique_ptr<sst::db::DbManager> mgr_;
    std::unique_ptr<FakeRecordingService> recording_;
    std::unique_ptr<MatchService> service_;
    std::int64_t soccer_id_{0};
    std::int64_t team_a_id_{0};
    std::int64_t team_b_id_{0};
};

// ── StartMatch ─────────────────────────────────────────────────────────

TEST_F(MatchServiceTest, StartMatchInsertsRowAndDelegatesToRecording) {
    EXPECT_FALSE(service_->IsActive());

    auto res = service_->StartMatch({.sport_code = "soccer",
                                     .team_a_id = team_a_id_,
                                     .team_b_id = team_b_id_,
                                     .name = "Final"});
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.match_id.empty());
    EXPECT_FALSE(res.recording_id.empty());

    EXPECT_TRUE(service_->IsActive());
    EXPECT_EQ(service_->ActiveMatchId(), res.match_id);
    EXPECT_EQ(recording_->start_calls, 1);
    EXPECT_EQ(recording_->last_match_id, res.match_id);

    auto persisted = mgr_->matches().get(res.match_id);
    ASSERT_TRUE(persisted.success);
    EXPECT_EQ(persisted.data.sport_id, soccer_id_);
    EXPECT_EQ(persisted.data.team_a_id, team_a_id_);
    EXPECT_EQ(persisted.data.team_b_id, team_b_id_);
    ASSERT_TRUE(persisted.data.name.has_value());
    EXPECT_EQ(*persisted.data.name, "Final");
    EXPECT_FALSE(persisted.data.ended_at.has_value());
}

TEST_F(MatchServiceTest, StartMatchRejectsUnknownSport) {
    auto res = service_->StartMatch(
        {.sport_code = "underwater_basket_weaving",
         .team_a_id = team_a_id_,
         .team_b_id = team_b_id_});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->start_calls, 0);
    EXPECT_FALSE(service_->IsActive());
}

TEST_F(MatchServiceTest, StartMatchRejectsSameTeamBothSides) {
    auto res =
        service_->StartMatch({.sport_code = "soccer", .team_a_id = team_a_id_,
                              .team_b_id = team_a_id_});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->start_calls, 0);
}

TEST_F(MatchServiceTest, StartMatchRejectsTeamFromDifferentSport) {
    // Add a basketball sport + team. Soccer match with a basketball team must
    // be rejected by the composite FK on (team_id, sport_id).
    auto basketball =
        mgr_->sports().save({.code = "basketball", .name = "Basketball", .periods = 4});
    ASSERT_TRUE(basketball.success);
    auto bball_team = mgr_->teams().save(
        {.sport_id = basketball.data.id, .name = "Hoops", .short_name = "HPS"});
    ASSERT_TRUE(bball_team.success);

    auto res = service_->StartMatch({.sport_code = "soccer",
                                     .team_a_id = team_a_id_,
                                     .team_b_id = bball_team.data.id});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->start_calls, 0);
}

TEST_F(MatchServiceTest, StartMatchRollsBackWhenRecordingRefuses) {
    recording_->accept_start = false;

    auto res = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(service_->IsActive());

    // The match row was inserted then removed; no orphan rows survive.
    auto remaining = mgr_->matches().listByUser(kDefaultUserId);
    ASSERT_TRUE(remaining.success);
    EXPECT_TRUE(remaining.data.empty());
}

TEST_F(MatchServiceTest, DoubleStartRejected) {
    ASSERT_TRUE(service_
                    ->StartMatch({.sport_code = "soccer", .team_a_id = team_a_id_,
                                  .team_b_id = team_b_id_})
                    .success);
    auto res2 = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    EXPECT_FALSE(res2.success);
    EXPECT_EQ(recording_->start_calls, 1);
}

// ── SetPeriod / SetScore ───────────────────────────────────────────────

TEST_F(MatchServiceTest, SetPeriodPersistsAndRejectsBadValues) {
    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);

    EXPECT_TRUE(service_->SetPeriod(2));
    EXPECT_FALSE(service_->SetPeriod(0));
    EXPECT_FALSE(service_->SetPeriod(-1));

    auto reloaded = mgr_->matches().get(start.match_id);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.current_period, 2);
}

TEST_F(MatchServiceTest, SetPeriodRejectedWhenIdle) {
    EXPECT_FALSE(service_->SetPeriod(1));
}

TEST_F(MatchServiceTest, SetScoreUpdatesIndependentSidesAndPersists) {
    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);

    EXPECT_TRUE(service_->SetScore({.team = TeamSide::kA, .score = 3}));
    EXPECT_TRUE(service_->SetScore({.team = TeamSide::kB, .score = 1}));
    EXPECT_FALSE(service_->SetScore({.team = TeamSide::kA, .score = -1}));

    auto reloaded = mgr_->matches().get(start.match_id);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.final_score_a, 3);
    EXPECT_EQ(reloaded.data.final_score_b, 1);
}

// ── RecordEvent ────────────────────────────────────────────────────────

TEST_F(MatchServiceTest, RecordEventInsertsRowAndDelegatesWithCameraConfigWindow) {
    // Tighten event-clip window so the test asserts a non-default value comes
    // through end-to-end.
    sst::db::CameraConfig cam = mgr_->cameras().getConfig(kDefaultUserId).data;
    cam.event_clip_pre_seconds = 45;
    cam.event_clip_post_seconds = 90;
    ASSERT_TRUE(mgr_->cameras().saveConfig(cam).success);

    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);

    // Tiny sleep so timestamp_in_match > 0 in the event row.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto res = service_->RecordEvent({.event_code = "goal",
                                      .metadata_json = std::string{R"({"player":9})"}});
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.match_event_id.empty());
    EXPECT_FALSE(res.event_clip_id.empty());

    EXPECT_EQ(recording_->trigger_calls, 1);
    EXPECT_EQ(recording_->last_event_id, res.match_event_id);
    EXPECT_EQ(recording_->last_window.pre_seconds, 45);
    EXPECT_EQ(recording_->last_window.post_seconds, 90);

    auto event_row = mgr_->matches().getEvent(res.match_event_id);
    ASSERT_TRUE(event_row.success);
    EXPECT_EQ(event_row.data.match_id, start.match_id);
    EXPECT_EQ(event_row.data.event_code, "goal");
    EXPECT_EQ(event_row.data.period, 1);
    EXPECT_GT(event_row.data.timestamp_in_match, 0.0);
    ASSERT_TRUE(event_row.data.metadata_json.has_value());
}

TEST_F(MatchServiceTest, RecordEventRejectedWhenIdle) {
    auto res = service_->RecordEvent({.event_code = "goal"});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->trigger_calls, 0);
}

TEST_F(MatchServiceTest, RecordEventRejectedForUnknownEventCode) {
    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);

    auto res = service_->RecordEvent({.event_code = "slam_dunk"});  // basketball only
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->trigger_calls, 0);

    auto events = mgr_->matches().listEvents(start.match_id);
    ASSERT_TRUE(events.success);
    EXPECT_TRUE(events.data.empty());
}

// ── EndMatch ───────────────────────────────────────────────────────────

TEST_F(MatchServiceTest, EndMatchFinalizesDbAndStopsRecording) {
    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);
    const auto match_id = start.match_id;

    auto res = service_->EndMatch();
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.merged_path.empty());

    EXPECT_FALSE(service_->IsActive());
    EXPECT_TRUE(service_->ActiveMatchId().empty());
    EXPECT_EQ(recording_->stop_calls, 1);

    auto reloaded = mgr_->matches().get(match_id);
    ASSERT_TRUE(reloaded.success);
    ASSERT_TRUE(reloaded.data.ended_at.has_value());
}

TEST_F(MatchServiceTest, EndMatchRejectedWhenIdle) {
    auto res = service_->EndMatch();
    EXPECT_FALSE(res.success);
    EXPECT_EQ(recording_->stop_calls, 0);
}

TEST_F(MatchServiceTest, FullCycleStartRecordEventsEnd) {
    auto start = service_->StartMatch(
        {.sport_code = "soccer", .team_a_id = team_a_id_, .team_b_id = team_b_id_});
    ASSERT_TRUE(start.success);

    ASSERT_TRUE(service_->SetPeriod(1));
    ASSERT_TRUE(service_->RecordEvent({.event_code = "goal"}).success);
    ASSERT_TRUE(service_->SetScore({.team = TeamSide::kA, .score = 1}));

    ASSERT_TRUE(service_->SetPeriod(2));
    ASSERT_TRUE(service_->RecordEvent({.event_code = "yellow_card"}).success);

    ASSERT_TRUE(service_->EndMatch().success);

    auto events = mgr_->matches().listEvents(start.match_id);
    ASSERT_TRUE(events.success);
    EXPECT_EQ(events.data.size(), 2);
    EXPECT_EQ(events.data[0].event_code, "goal");
    EXPECT_EQ(events.data[1].event_code, "yellow_card");
    EXPECT_EQ(events.data[1].period, 2);

    auto reloaded = mgr_->matches().get(start.match_id);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.final_score_a, 1);
    ASSERT_TRUE(reloaded.data.ended_at.has_value());
}

}  // namespace
