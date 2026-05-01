#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"
#include "domain/common/utils/uuid.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/config-data.hpp"
#include "domain/config/models/wifi-direct.hpp"
#include "domain/db/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace {

constexpr const char* kSchemaPath = SST_REPO_ROOT_DIR "/db/schema.sql";
constexpr int64_t kDefaultUserId = 1;
constexpr int64_t kMissingUserId = 9999;
constexpr int32_t kSeededDeviceId = 0;
constexpr int32_t kMissingDeviceId = 99;

// Per-test counter so each TEST_F gets a unique on-disk SQLite file. Lets the
// suite run repeatedly without leftover state, and (in principle) in parallel.
auto NextDbPath() -> std::string {
    static std::atomic<int> counter{0};
    const int n = counter.fetch_add(1);
    const auto path = std::filesystem::path(SST_REPO_ROOT_DIR) /
                      ("tests/db/data/test_" + std::to_string(::getpid()) + "_" +
                       std::to_string(n) + ".db");
    return path.string();
}

auto MakeSeedConfig() -> sst::config::ConfigData {
    sst::config::CalibrationCameraDeviceData cam_dev;
    cam_dev.id = 0;
    cam_dev.intrinsic_matrix = std::array<float, 9>{1.F, 0.F, 0.F, 0.F, 1.F, 0.F, 0.F, 0.F, 1.F};
    cam_dev.distortion_coefficients = std::array<float, 5>{0.F, 0.F, 0.F, 0.F, 0.F};

    sst::config::CalibrationCamerasData cams;
    cams.device = std::vector{cam_dev};

    sst::config::CalibrationMicrophoneDeviceData mic_dev;
    mic_dev.id = 0;
    mic_dev.sensitivity = 1.F;

    sst::config::CalibrationMicrophonesData mics;
    mics.device = std::vector{mic_dev};

    sst::config::WifiDirectData wifi_direct;
    wifi_direct.enabled = true;
    wifi_direct.ssid = "DIRECT-SST-CAM";
    wifi_direct.passphrase = "scoutcam1234";
    wifi_direct.channel = 6U;
    wifi_direct.ip_address = "192.168.49.1";

    sst::config::ConfigData cfg;
    cfg.calibration.cameras = cams;
    cfg.calibration.microphones = mics;
    cfg.wifi_direct = wifi_direct;
    return cfg;
}

class DbTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        db_path_ = NextDbPath();
        const std::filesystem::path p{db_path_};
        std::filesystem::create_directories(p.parent_path());
        std::filesystem::remove(p);
        spdlog::info("DbTest: starting fresh DB at {}", db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(
            sst::db::DbManager::Config{.db_path = db_path_, .schema_path = kSchemaPath});

        sst::db::DbSeeder::seedIfEmpty(*mgr_, MakeSeedConfig());
        spdlog::info("DbTest: seed complete, ready for reads");
    }

    auto TearDown() -> void override {
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    // Inserts a "soccer" sport + its event kinds. Returns the sport id.
    auto SeedSoccerSport() -> int64_t {
        sst::db::Sport soccer{.code = "soccer", .name = "Soccer", .periods = 2};
        const auto saved = mgr_->sports().save(soccer);
        EXPECT_TRUE(saved.success);
        const int64_t sport_id = saved.data.id;
        for (const auto& [code, name] : std::vector<std::pair<std::string, std::string>>{
                 {"goal", "Goal"},
                 {"yellow_card", "Yellow Card"},
                 {"red_card", "Red Card"}}) {
            EXPECT_TRUE(mgr_->sports()
                            .saveEventKind({.sport_id = sport_id, .code = code, .name = name})
                            .success);
        }
        return sport_id;
    }

    // Inserts two teams under a sport. Returns their ids.
    auto SeedTwoTeams(int64_t sport_id) -> std::pair<int64_t, int64_t> {
        const auto a = mgr_->teams().save(
            {.sport_id = sport_id, .name = "Alpha FC", .short_name = "ALP"});
        const auto b = mgr_->teams().save(
            {.sport_id = sport_id, .name = "Bravo FC", .short_name = "BRV"});
        EXPECT_TRUE(a.success);
        EXPECT_TRUE(b.success);
        return {a.data.id, b.data.id};
    }

    std::unique_ptr<sst::db::DbManager> mgr_;
    std::string db_path_;
};

TEST_F(DbTest, UserRead) {
    auto known = mgr_->users().get(kDefaultUserId);
    ASSERT_TRUE(known.success);
    spdlog::info("User:\n{}", known.data);
    EXPECT_FALSE(mgr_->users().get(kMissingUserId).success);
}

TEST_F(DbTest, CameraRead) {
    auto cfg = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(cfg.success);
    spdlog::info("CameraConfig:\n{}", cfg.data);
    EXPECT_FALSE(mgr_->cameras().getConfig(kMissingUserId).success);

    auto cal = mgr_->cameras().getLatestCalibration(kSeededDeviceId);
    ASSERT_TRUE(cal.success);
    spdlog::info("CameraCalibration:\n{}", cal.data);
    EXPECT_FALSE(mgr_->cameras().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, CameraConfigEventClipFieldsRoundTrip) {
    // Seeded defaults from the schema.
    auto seeded = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(seeded.success);
    EXPECT_EQ(seeded.data.event_clip_pre_seconds,
              sst::db::CameraConfig::kDefaultEventClipPreSeconds);
    EXPECT_EQ(seeded.data.event_clip_post_seconds,
              sst::db::CameraConfig::kDefaultEventClipPostSeconds);

    // Custom values round-trip.
    sst::db::CameraConfig updated = seeded.data;
    updated.event_clip_pre_seconds = 45;
    updated.event_clip_post_seconds = 90;
    ASSERT_TRUE(mgr_->cameras().saveConfig(updated).success);

    auto reloaded = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.event_clip_pre_seconds, 45);
    EXPECT_EQ(reloaded.data.event_clip_post_seconds, 90);
}

TEST_F(DbTest, MicrophoneRead) {
    auto cfg = mgr_->microphones().getConfig(kDefaultUserId);
    ASSERT_TRUE(cfg.success);
    EXPECT_FALSE(mgr_->microphones().getConfig(kMissingUserId).success);

    auto cal = mgr_->microphones().getLatestCalibration(kSeededDeviceId);
    ASSERT_TRUE(cal.success);
    EXPECT_FALSE(mgr_->microphones().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, NetworkRead) {
    auto wifi_direct = mgr_->network().getWifiDirect(kDefaultUserId);
    ASSERT_TRUE(wifi_direct.success);
    EXPECT_EQ(wifi_direct.data.ssid, "DIRECT-SST-CAM");
    EXPECT_FALSE(mgr_->network().getWifiDirect(kMissingUserId).success);

    auto bluetooth = mgr_->network().getBluetooth(kDefaultUserId);
    ASSERT_TRUE(bluetooth.success);
    EXPECT_FALSE(mgr_->network().getBluetooth(kMissingUserId).success);
}

TEST_F(DbTest, StreamRead) {
    auto res = mgr_->streams().getAll(kDefaultUserId);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.empty());
}

TEST_F(DbTest, StreamConfigCompanionAppHasNullableKeyAndUrl) {
    sst::db::StreamConfig companion;
    companion.user_id = kDefaultUserId;
    companion.platform = sst::db::StreamPlatform::kCompanionApp;
    companion.name = "companion-app";
    companion.enabled = true;
    companion.width = 854;
    companion.height = 480;
    companion.framerate = 30;
    companion.bitrate_kbps = 1500;
    // stream_key + url remain nullopt — schema permits this only for kCompanionApp.

    const auto saved = mgr_->streams().save(companion);
    ASSERT_TRUE(saved.success);
    EXPECT_GT(saved.data.id, 0);

    const auto all = mgr_->streams().getAll(kDefaultUserId);
    ASSERT_TRUE(all.success);
    ASSERT_EQ(all.data.size(), 1);
    EXPECT_EQ(all.data.front().platform, sst::db::StreamPlatform::kCompanionApp);
    EXPECT_FALSE(all.data.front().stream_key.has_value());
    EXPECT_FALSE(all.data.front().url.has_value());
}

TEST_F(DbTest, StreamConfigPlatformRowRequiresKeyAndUrl) {
    sst::db::StreamConfig youtube;
    youtube.user_id = kDefaultUserId;
    youtube.platform = sst::db::StreamPlatform::kYouTube;
    youtube.name = "main";
    youtube.stream_key = "abcd-1234";
    youtube.url = "rtmp://a.rtmp.youtube.com/live2";
    ASSERT_TRUE(mgr_->streams().save(youtube).success);

    sst::db::StreamConfig bad;
    bad.user_id = kDefaultUserId;
    bad.platform = sst::db::StreamPlatform::kYouTube;
    bad.name = "missing-creds";
    // stream_key + url left null — CHECK constraint must reject.
    EXPECT_FALSE(mgr_->streams().save(bad).success);
}

TEST_F(DbTest, SportCrud) {
    const int64_t sport_id = SeedSoccerSport();
    EXPECT_GT(sport_id, 0);

    auto by_code = mgr_->sports().getByCode("soccer");
    ASSERT_TRUE(by_code.success);
    EXPECT_EQ(by_code.data.id, sport_id);
    EXPECT_EQ(by_code.data.periods, 2);

    auto by_id = mgr_->sports().get(sport_id);
    ASSERT_TRUE(by_id.success);
    EXPECT_EQ(by_id.data.code, "soccer");

    auto kinds = mgr_->sports().listEventKinds(sport_id);
    ASSERT_TRUE(kinds.success);
    EXPECT_EQ(kinds.data.size(), 3);

    auto goal = mgr_->sports().getEventKind(sport_id, "goal");
    ASSERT_TRUE(goal.success);
    EXPECT_EQ(goal.data.name, "Goal");

    EXPECT_TRUE(mgr_->sports().removeEventKind(sport_id, "yellow_card").data);
    auto after_remove = mgr_->sports().listEventKinds(sport_id);
    ASSERT_TRUE(after_remove.success);
    EXPECT_EQ(after_remove.data.size(), 2);
}

TEST_F(DbTest, TeamAndPlayerCrud) {
    const int64_t sport_id = SeedSoccerSport();
    const auto [team_a, team_b] = SeedTwoTeams(sport_id);
    EXPECT_NE(team_a, team_b);

    auto teams = mgr_->teams().list(sport_id);
    ASSERT_TRUE(teams.success);
    EXPECT_EQ(teams.data.size(), 2);

    auto fetched = mgr_->teams().get(team_a);
    ASSERT_TRUE(fetched.success);
    EXPECT_EQ(fetched.data.short_name, "ALP");

    const auto player1 =
        mgr_->teams().savePlayer({.team_id = team_a, .name = "Striker", .jersey_number = 9});
    const auto player_no_jersey =
        mgr_->teams().savePlayer({.team_id = team_a, .name = "Reserve"});
    ASSERT_TRUE(player1.success);
    ASSERT_TRUE(player_no_jersey.success);

    auto roster = mgr_->teams().listPlayers(team_a);
    ASSERT_TRUE(roster.success);
    EXPECT_EQ(roster.data.size(), 2);

    // The player without a jersey number round-trips as nullopt.
    auto refetched_no_jersey = mgr_->teams().getPlayer(player_no_jersey.data.id);
    ASSERT_TRUE(refetched_no_jersey.success);
    EXPECT_FALSE(refetched_no_jersey.data.jersey_number.has_value());

    EXPECT_TRUE(mgr_->teams().removePlayer(player1.data.id).data);
    auto roster_after = mgr_->teams().listPlayers(team_a);
    ASSERT_TRUE(roster_after.success);
    EXPECT_EQ(roster_after.data.size(), 1);
}

TEST_F(DbTest, MatchAcceptsTeamsOfSameSport) {
    const int64_t sport_id = SeedSoccerSport();
    const auto [team_a, team_b] = SeedTwoTeams(sport_id);

    sst::db::Match m;
    m.id = sst::common::utils::MakeUuidV4();
    m.user_id = kDefaultUserId;
    m.sport_id = sport_id;
    m.team_a_id = team_a;
    m.team_b_id = team_b;
    m.name = "Final";
    m.started_at = "2026-05-01T20:00:00";
    ASSERT_TRUE(mgr_->matches().save(m).success);

    auto reloaded = mgr_->matches().get(m.id);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.user_id, kDefaultUserId);
    EXPECT_EQ(reloaded.data.team_a_id, team_a);
    EXPECT_EQ(reloaded.data.team_b_id, team_b);
    EXPECT_FALSE(reloaded.data.ended_at.has_value());

    EXPECT_TRUE(mgr_->matches().updatePeriod(m.id, 2).data);
    EXPECT_TRUE(mgr_->matches().updateScore(m.id, 1, 2).data);
    EXPECT_TRUE(mgr_->matches().finalize(m.id, "2026-05-01T22:00:00").data);

    auto finalized = mgr_->matches().get(m.id);
    ASSERT_TRUE(finalized.success);
    EXPECT_EQ(finalized.data.current_period, 2);
    EXPECT_EQ(finalized.data.final_score_a, 1);
    EXPECT_EQ(finalized.data.final_score_b, 2);
    ASSERT_TRUE(finalized.data.ended_at.has_value());
    EXPECT_EQ(*finalized.data.ended_at, "2026-05-01T22:00:00");

    auto by_user = mgr_->matches().listByUser(kDefaultUserId);
    ASSERT_TRUE(by_user.success);
    EXPECT_EQ(by_user.data.size(), 1);
}

TEST_F(DbTest, MatchRejectsTeamsOfDifferentSports) {
    const int64_t soccer_id = SeedSoccerSport();
    const auto [soccer_a, soccer_b] = SeedTwoTeams(soccer_id);

    // Add basketball + a basketball team — composite FK on match must reject
    // any pairing of teams from different sports.
    auto basketball =
        mgr_->sports().save({.code = "basketball", .name = "Basketball", .periods = 4});
    ASSERT_TRUE(basketball.success);
    auto bball_team =
        mgr_->teams().save({.sport_id = basketball.data.id, .name = "Hoops", .short_name = "HPS"});
    ASSERT_TRUE(bball_team.success);

    sst::db::Match m;
    m.id = sst::common::utils::MakeUuidV4();
    m.user_id = kDefaultUserId;
    m.sport_id = soccer_id;       // soccer match …
    m.team_a_id = soccer_a;
    m.team_b_id = bball_team.data.id;  // … but team_b is a basketball team
    m.started_at = "2026-05-01T20:00:00";
    EXPECT_FALSE(mgr_->matches().save(m).success);
    (void)soccer_b;
}

TEST_F(DbTest, MatchEventConstrainedBySport) {
    const int64_t sport_id = SeedSoccerSport();
    const auto [team_a, team_b] = SeedTwoTeams(sport_id);

    sst::db::Match m;
    m.id = sst::common::utils::MakeUuidV4();
    m.user_id = kDefaultUserId;
    m.sport_id = sport_id;
    m.team_a_id = team_a;
    m.team_b_id = team_b;
    m.started_at = "2026-05-01T20:00:00";
    ASSERT_TRUE(mgr_->matches().save(m).success);

    sst::db::MatchEvent goal;
    goal.id = sst::common::utils::MakeUuidV4();
    goal.match_id = m.id;
    goal.sport_id = sport_id;
    goal.event_code = "goal";
    goal.period = 1;
    goal.timestamp_in_match = 600.0;
    goal.wall_clock_at = "2026-05-01T20:10:00";
    goal.metadata_json = R"({"scorer_player_id":9})";
    ASSERT_TRUE(mgr_->matches().saveEvent(goal).success);

    auto fetched_event = mgr_->matches().getEvent(goal.id);
    ASSERT_TRUE(fetched_event.success);
    EXPECT_EQ(fetched_event.data.event_code, "goal");
    EXPECT_EQ(fetched_event.data.period, 1);
    EXPECT_DOUBLE_EQ(fetched_event.data.timestamp_in_match, 600.0);
    ASSERT_TRUE(fetched_event.data.metadata_json.has_value());

    sst::db::MatchEvent slam_dunk;
    slam_dunk.id = sst::common::utils::MakeUuidV4();
    slam_dunk.match_id = m.id;
    slam_dunk.sport_id = sport_id;
    slam_dunk.event_code = "slam_dunk";  // not a soccer event
    slam_dunk.period = 1;
    slam_dunk.timestamp_in_match = 1.0;
    slam_dunk.wall_clock_at = "2026-05-01T20:00:01";
    EXPECT_FALSE(mgr_->matches().saveEvent(slam_dunk).success);

    auto events = mgr_->matches().listEvents(m.id);
    ASSERT_TRUE(events.success);
    EXPECT_EQ(events.data.size(), 1);
}

TEST_F(DbTest, RecordingFullGameWithSegments) {
    const int64_t sport_id = SeedSoccerSport();
    const auto [team_a, team_b] = SeedTwoTeams(sport_id);

    sst::db::Match m;
    m.id = sst::common::utils::MakeUuidV4();
    m.user_id = kDefaultUserId;
    m.sport_id = sport_id;
    m.team_a_id = team_a;
    m.team_b_id = team_b;
    m.started_at = "2026-05-01T20:00:00";
    ASSERT_TRUE(mgr_->matches().save(m).success);

    sst::db::Recording rec;
    rec.id = sst::common::utils::MakeUuidV4();
    rec.match_id = m.id;
    rec.kind = sst::db::RecordingKind::kFullGame;
    rec.file_path = "/var/lib/sst/cam/videos/" + m.id + "/full_game/full_game.mp4";
    rec.started_at = m.started_at;
    ASSERT_TRUE(mgr_->recordings().save(rec).success);

    for (int seq = 0; seq < 3; ++seq) {
        const auto saved = mgr_->recordings().saveSegment(
            {.recording_id = rec.id,
             .sequence = seq,
             .file_path = "/var/lib/sst/cam/videos/" + m.id + "/full_game/seg_" +
                          std::to_string(seq) + ".mp4",
             .started_at = "2026-05-01T20:0" + std::to_string(seq) + ":00"});
        ASSERT_TRUE(saved.success);
        EXPECT_GT(saved.data.id, 0);
    }

    auto segments = mgr_->recordings().listSegments(rec.id);
    ASSERT_TRUE(segments.success);
    EXPECT_EQ(segments.data.size(), 3);
    EXPECT_EQ(segments.data[0].sequence, 0);
    EXPECT_EQ(segments.data[2].sequence, 2);

    EXPECT_TRUE(mgr_->recordings().finalizeSegment(segments.data[0].id, "2026-05-01T20:01:00").data);
    EXPECT_TRUE(mgr_->recordings().finalize(rec.id, "2026-05-01T22:00:00").data);

    auto reloaded = mgr_->recordings().get(rec.id);
    ASSERT_TRUE(reloaded.success);
    ASSERT_TRUE(reloaded.data.ended_at.has_value());
}

TEST_F(DbTest, RecordingEventClipChainedToMatchEvent) {
    const int64_t sport_id = SeedSoccerSport();
    const auto [team_a, team_b] = SeedTwoTeams(sport_id);

    sst::db::Match m;
    m.id = sst::common::utils::MakeUuidV4();
    m.user_id = kDefaultUserId;
    m.sport_id = sport_id;
    m.team_a_id = team_a;
    m.team_b_id = team_b;
    m.started_at = "2026-05-01T20:00:00";
    ASSERT_TRUE(mgr_->matches().save(m).success);

    sst::db::MatchEvent goal;
    goal.id = sst::common::utils::MakeUuidV4();
    goal.match_id = m.id;
    goal.sport_id = sport_id;
    goal.event_code = "goal";
    goal.period = 1;
    goal.timestamp_in_match = 1234.5;
    goal.wall_clock_at = "2026-05-01T20:20:34";
    ASSERT_TRUE(mgr_->matches().saveEvent(goal).success);

    sst::db::Recording clip_recording;
    clip_recording.id = sst::common::utils::MakeUuidV4();
    clip_recording.match_id = m.id;
    clip_recording.kind = sst::db::RecordingKind::kEventClip;
    clip_recording.file_path =
        "/var/lib/sst/cam/videos/" + m.id + "/event_" + goal.id + "/clip.mp4";
    clip_recording.started_at = goal.wall_clock_at;
    ASSERT_TRUE(mgr_->recordings().save(clip_recording).success);

    sst::db::EventClip clip;
    clip.id = sst::common::utils::MakeUuidV4();
    clip.match_event_id = goal.id;
    clip.recording_id = clip_recording.id;
    clip.file_path = clip_recording.file_path;
    clip.pre_seconds = 60;
    clip.post_seconds = 60;
    ASSERT_TRUE(mgr_->recordings().saveEventClip(clip).success);

    auto fetched = mgr_->recordings().getEventClip(clip.id);
    ASSERT_TRUE(fetched.success);
    EXPECT_EQ(fetched.data.match_event_id, goal.id);
    EXPECT_EQ(fetched.data.pre_seconds, 60);

    auto for_match = mgr_->recordings().listEventClipsForMatch(m.id);
    ASSERT_TRUE(for_match.success);
    EXPECT_EQ(for_match.data.size(), 1);
}

}  // namespace
