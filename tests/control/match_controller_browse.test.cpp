// Read-only browse-verb tests for MatchController. The write verbs (start /
// set_period / set_score / record_event / end) are exercised end-to-end by
// MatchServiceTest; these tests focus on the controller's serialization and
// repo-lookup paths for list / get / list_clips / get_clip.

#include <gtest/gtest.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/control/services/controllers/match.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/match/ports/match-service.hpp"
#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"
#include "domain/db/models/match.hpp"
#include "domain/db/models/recording.hpp"

namespace {

using sst::control::Command;
using sst::control::CommandStatus;
using sst::control::MatchController;

constexpr std::int64_t kDefaultUserId = 1;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

auto MakeCommand(std::string_view text) -> Command {
    Command cmd;
    cmd.route = "match";
    cmd.payload.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(text[i]);
    }
    return cmd;
}

auto PayloadToString(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// Browse verbs don't call the service, but the controller still requires one.
// This stub never gets exercised in these tests.
class UnusedMatchService final : public sst::match::IMatchService {
   public:
    auto StartMatch(const sst::match::StartMatchRequest&)
        -> sst::match::StartMatchResult override {
        return {};
    }
    auto SetPeriod(std::int32_t) -> bool override { return false; }
    auto SetScore(const sst::match::SetScoreRequest&) -> bool override { return false; }
    auto RecordEvent(const sst::match::RecordEventRequest&)
        -> sst::match::RecordEventResult override {
        return {};
    }
    auto EndMatch() -> sst::match::EndMatchResult override { return {}; }
    auto IsActive() const -> bool override { return false; }
    auto ActiveMatchId() const -> std::string override { return {}; }
};

class MatchControllerBrowseTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/control/data/match_browse_" + suffix + ".db"))
                       .string();
        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });
        ASSERT_TRUE(mgr_->users().create("test").success);

        soccer_id_ = mgr_->sports().save({.code = "soccer", .name = "Soccer", .periods = 2}).data.id;
        ASSERT_TRUE(mgr_->sports()
                        .saveEventKind({.sport_id = soccer_id_, .code = "goal", .name = "Goal"})
                        .success);
        team_a_id_ = mgr_->teams()
                         .save({.sport_id = soccer_id_, .name = "Alpha", .short_name = "A"})
                         .data.id;
        team_b_id_ = mgr_->teams()
                         .save({.sport_id = soccer_id_, .name = "Bravo", .short_name = "B"})
                         .data.id;

        controller_ = std::make_unique<MatchController>(stub_service_, mgr_->matches(),
                                                        mgr_->recordings(), kDefaultUserId);
    }

    auto TearDown() -> void override {
        controller_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    auto SeedMatch(const std::string& id, const std::string& started_at,
                   const std::string& name) -> void {
        sst::db::Match m;
        m.id = id;
        m.user_id = kDefaultUserId;
        m.sport_id = soccer_id_;
        m.team_a_id = team_a_id_;
        m.team_b_id = team_b_id_;
        m.name = name;
        m.started_at = started_at;
        ASSERT_TRUE(mgr_->matches().save(m).success);
    }

    auto SeedEvent(const std::string& match_id, const std::string& event_id,
                   const std::string& code, double timestamp_in_match) -> void {
        sst::db::MatchEvent e;
        e.id = event_id;
        e.match_id = match_id;
        e.sport_id = soccer_id_;
        e.event_code = code;
        e.period = 1;
        e.timestamp_in_match = timestamp_in_match;
        e.wall_clock_at = "2026-05-02T20:00:00";
        e.metadata_json = std::string{R"({"player":9})"};
        ASSERT_TRUE(mgr_->matches().saveEvent(e).success);
    }

    // Inserts a recording row (kind=event_clip) + event_clip row pointing to it.
    auto SeedEventClip(const std::string& clip_id, const std::string& match_event_id) -> void {
        sst::db::Recording rec;
        rec.id = "rec-" + clip_id;
        rec.match_id = std::nullopt;  // event clips don't strictly need to ref the match here
        rec.kind = sst::db::RecordingKind::kEventClip;
        rec.file_path = "/var/lib/sst/cam/videos/clip_" + clip_id + ".mp4";
        rec.started_at = "2026-05-02T20:00:01";
        ASSERT_TRUE(mgr_->recordings().save(rec).success);

        sst::db::EventClip clip;
        clip.id = clip_id;
        clip.match_event_id = match_event_id;
        clip.recording_id = rec.id;
        clip.file_path = rec.file_path;
        clip.pre_seconds = 60;
        clip.post_seconds = 90;
        ASSERT_TRUE(mgr_->recordings().saveEventClip(clip).success);
    }

    std::string db_path_;
    std::unique_ptr<sst::db::DbManager> mgr_;
    UnusedMatchService stub_service_;
    std::unique_ptr<MatchController> controller_;
    std::int64_t soccer_id_{0};
    std::int64_t team_a_id_{0};
    std::int64_t team_b_id_{0};
};

TEST_F(MatchControllerBrowseTest, ListEmptyReturnsEmptyArray) {
    auto res = controller_->Handle(MakeCommand("list"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    ASSERT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 0U);
}

TEST_F(MatchControllerBrowseTest, ListReturnsMatchesNewestFirst) {
    SeedMatch("m1", "2026-05-01T20:00:00", "Earlier");
    SeedMatch("m2", "2026-05-02T20:00:00", "Later");

    auto res = controller_->Handle(MakeCommand("list"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    ASSERT_EQ(j.size(), 2U);
    EXPECT_EQ(j[0]["id"].get<std::string>(), "m2");
    EXPECT_EQ(j[1]["id"].get<std::string>(), "m1");
    EXPECT_EQ(j[0]["name"].get<std::string>(), "Later");
}

TEST_F(MatchControllerBrowseTest, GetReturnsMatchAndEventsOrderedByTimestamp) {
    SeedMatch("m1", "2026-05-02T20:00:00", "Final");
    SeedEvent("m1", "e2", "goal", 600.0);
    SeedEvent("m1", "e1", "goal", 120.0);

    auto res = controller_->Handle(MakeCommand("get m1"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));

    EXPECT_EQ(j["match"]["id"].get<std::string>(), "m1");
    ASSERT_EQ(j["events"].size(), 2U);
    EXPECT_EQ(j["events"][0]["id"].get<std::string>(), "e1");  // 120.0
    EXPECT_EQ(j["events"][1]["id"].get<std::string>(), "e2");  // 600.0
    // metadata round-trips as structured JSON, not a string.
    EXPECT_EQ(j["events"][0]["metadata"]["player"].get<int>(), 9);
}

TEST_F(MatchControllerBrowseTest, GetReturnsNotFoundForUnknownMatch) {
    auto res = controller_->Handle(MakeCommand("get does-not-exist"));
    EXPECT_EQ(res.status, CommandStatus::kNotFound);
}

TEST_F(MatchControllerBrowseTest, GetRequiresMatchId) {
    auto res = controller_->Handle(MakeCommand("get"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(MatchControllerBrowseTest, ListClipsReturnsClipsForMatch) {
    SeedMatch("m1", "2026-05-02T20:00:00", "Final");
    SeedEvent("m1", "e1", "goal", 120.0);
    SeedEvent("m1", "e2", "goal", 600.0);
    SeedEventClip("c1", "e1");
    SeedEventClip("c2", "e2");

    auto res = controller_->Handle(MakeCommand("list_clips m1"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    ASSERT_EQ(j.size(), 2U);
    EXPECT_EQ(j[0]["id"].get<std::string>(), "c1");
    EXPECT_EQ(j[0]["pre_seconds"].get<int>(), 60);
    EXPECT_EQ(j[1]["id"].get<std::string>(), "c2");
}

TEST_F(MatchControllerBrowseTest, ListClipsReturnsEmptyForMatchWithNoEvents) {
    SeedMatch("m1", "2026-05-02T20:00:00", "Final");

    auto res = controller_->Handle(MakeCommand("list_clips m1"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    EXPECT_EQ(j.size(), 0U);
}

TEST_F(MatchControllerBrowseTest, GetClipReturnsClipAndRecording) {
    SeedMatch("m1", "2026-05-02T20:00:00", "Final");
    SeedEvent("m1", "e1", "goal", 120.0);
    SeedEventClip("c1", "e1");

    auto res = controller_->Handle(MakeCommand("get_clip c1"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    EXPECT_EQ(j["clip"]["id"].get<std::string>(), "c1");
    EXPECT_EQ(j["clip"]["pre_seconds"].get<int>(), 60);
    EXPECT_EQ(j["clip"]["post_seconds"].get<int>(), 90);
    ASSERT_TRUE(j["recording"].is_object());
    EXPECT_EQ(j["recording"]["id"].get<std::string>(), "rec-c1");
    EXPECT_FALSE(j["recording"]["file_path"].get<std::string>().empty());
}

TEST_F(MatchControllerBrowseTest, GetClipReturnsNotFoundForUnknownClip) {
    auto res = controller_->Handle(MakeCommand("get_clip nope"));
    EXPECT_EQ(res.status, CommandStatus::kNotFound);
}

}  // namespace
