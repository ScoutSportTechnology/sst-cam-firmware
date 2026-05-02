#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "adapters/db/sqlite/db-connection.hpp"
#include "adapters/db/sqlite/recording-repository.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/storage/ports/disk-guard.hpp"
#include "app/storage/ports/encoder-pipeline.hpp"
#include "app/storage/ports/event-clip-recorder.hpp"
#include "app/storage/ports/segment-recorder.hpp"
#include "app/storage/services/recording_service/recording-service.hpp"
#include "domain/storage/models/event-clip-window.hpp"
#include "domain/storage/models/recording-state.hpp"

namespace {

using sst::storage::EncodedNal;
using sst::storage::EventClipWindow;
using sst::storage::IDiskGuard;
using sst::storage::IEncoderPipeline;
using sst::storage::IEventClipRecorder;
using sst::storage::ISegmentRecorder;
using sst::storage::RecordingService;
using sst::storage::RecordingState;

// Per-test DB + video root so each TEST_F is fully isolated.
auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

// ── Test doubles ──────────────────────────────────────────────────────────
//
// Stand in for the GStreamer adapters (which wrap NVENC and aren't available
// in the cross-compile container). Each captures method calls so the state-
// machine assertions can verify the service's interactions.

class FakeEncoderPipeline final : public IEncoderPipeline {
   public:
    auto SetSegmentSink(EncodedSink sink) -> void override { segment_sink_ = std::move(sink); }
    auto SetRingSink(EncodedSink sink) -> void override { ring_sink_ = std::move(sink); }

    auto Start() -> bool override {
        ++start_calls;
        if (!accept_start) {
            return false;
        }
        running_ = true;
        return true;
    }

    auto Stop() -> void override {
        ++stop_calls;
        running_ = false;
    }

    [[nodiscard]] auto IsRunning() const -> bool override { return running_; }

    auto Push(const sst::capture::Frame& /*frame*/) -> void override { ++push_calls; }

    int start_calls{0};
    int stop_calls{0};
    int push_calls{0};
    bool accept_start{true};

   private:
    bool running_{false};
    EncodedSink segment_sink_;
    EncodedSink ring_sink_;
};

class FakeSegmentRecorder final : public ISegmentRecorder {
   public:
    auto Start(const std::filesystem::path& output_dir) -> bool override {
        ++start_calls;
        last_output_dir = output_dir;
        if (!accept_start) {
            return false;
        }
        running_ = true;
        paused_ = false;
        emitted_segments_.clear();
        return true;
    }

    auto Pause() -> void override {
        ++pause_calls;
        paused_ = true;
    }

    auto Resume() -> void override {
        ++resume_calls;
        paused_ = false;
    }

    auto Stop() -> std::filesystem::path override {
        ++stop_calls;
        running_ = false;
        const auto merged = last_output_dir / "full_game.mp4";
        emitted_segments_ = {last_output_dir / "seg_00000.mp4",
                             last_output_dir / "seg_00001.mp4"};
        return merged;
    }

    [[nodiscard]] auto IsRunning() const -> bool override { return running_; }
    [[nodiscard]] auto IsPaused() const -> bool override { return paused_; }
    [[nodiscard]] auto Segments() const -> std::vector<std::filesystem::path> override {
        return emitted_segments_;
    }

    auto OnEncoded(const EncodedNal& /*nal*/) -> void override { ++on_encoded_calls; }

    int start_calls{0};
    int pause_calls{0};
    int resume_calls{0};
    int stop_calls{0};
    int on_encoded_calls{0};
    bool accept_start{true};
    std::filesystem::path last_output_dir;

   private:
    bool running_{false};
    bool paused_{false};
    std::vector<std::filesystem::path> emitted_segments_;
};

class FakeEventClipRecorder final : public IEventClipRecorder {
   public:
    auto OnEncoded(const EncodedNal& /*nal*/) -> void override { ++on_encoded_calls; }

    auto Trigger(const std::filesystem::path& output_path, const EventClipWindow& window)
        -> bool override {
        ++trigger_calls;
        last_output_path = output_path;
        last_window = window;
        return accept_next_trigger;
    }

    [[nodiscard]] auto IsBusy() const -> bool override { return false; }

    int on_encoded_calls{0};
    int trigger_calls{0};
    std::filesystem::path last_output_path;
    EventClipWindow last_window{};
    bool accept_next_trigger{true};
};

class FakeDiskGuard final : public IDiskGuard {
   public:
    [[nodiscard]] auto HasEnoughFreeSpace() const -> bool override { return has_space; }
    [[nodiscard]] auto FreeBytes() const -> std::uint64_t override { return free_bytes; }

    bool has_space{true};
    std::uint64_t free_bytes{1ULL << 40};  // 1 TiB
};

// ── Fixture ───────────────────────────────────────────────────────────────

class RecordingServiceTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/storage/data/test_" + suffix + ".db"))
                       .string();
        video_root_ = std::filesystem::path(SST_REPO_ROOT_DIR) /
                      ("tests/storage/data/video_" + suffix);

        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);
        std::error_code ec;
        std::filesystem::remove_all(video_root_, ec);
        std::filesystem::create_directories(video_root_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });

        // The recording table FKs to match.id; for tests that exercise
        // RecordingService::StartFullGame with a match_id, we either need a
        // real match row or to use a match_id of "" (FK NULL is allowed).
        // We seed a real match per test that uses one.

        encoder_owner_ = std::make_unique<FakeEncoderPipeline>();
        segment_owner_ = std::make_unique<FakeSegmentRecorder>();
        event_clip_owner_ = std::make_unique<FakeEventClipRecorder>();
        encoder_ = encoder_owner_.get();
        segment_recorder_ = segment_owner_.get();
        event_clip_recorder_ = event_clip_owner_.get();

        service_ = std::make_unique<RecordingService>(
            std::move(encoder_owner_), std::move(segment_owner_), std::move(event_clip_owner_),
            mgr_->recordings(), disk_guard_, video_root_);
    }

    auto TearDown() -> void override {
        service_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
        std::filesystem::remove_all(video_root_, ec);
    }

    // Inserts a sport + two teams + a match so RecordingService can persist
    // recording rows without violating the FK on match.id.
    auto SeedMatch(const std::string& match_id) -> void {
        auto sport = mgr_->sports().save({.code = "soccer", .name = "Soccer", .periods = 2});
        ASSERT_TRUE(sport.success);
        auto team_a = mgr_->teams().save(
            {.sport_id = sport.data.id, .name = "Alpha", .short_name = "A"});
        auto team_b = mgr_->teams().save(
            {.sport_id = sport.data.id, .name = "Bravo", .short_name = "B"});
        ASSERT_TRUE(team_a.success);
        ASSERT_TRUE(team_b.success);
        sst::db::Match m;
        m.id = match_id;
        m.user_id = 1;
        m.sport_id = sport.data.id;
        m.team_a_id = team_a.data.id;
        m.team_b_id = team_b.data.id;
        m.started_at = "2026-05-01T20:00:00";
        ASSERT_TRUE(mgr_->users().create("test").success);
        ASSERT_TRUE(mgr_->matches().save(m).success);
        seeded_sport_id_ = sport.data.id;
    }

    auto SeedSoccerEventKind(const std::string& code, const std::string& name) -> void {
        ASSERT_TRUE(mgr_->sports()
                        .saveEventKind({.sport_id = seeded_sport_id_, .code = code, .name = name})
                        .success);
    }

    // Inserts a match_event so TriggerEventClip can FK to it via event_clip.
    auto SeedMatchEvent(const std::string& match_id, const std::string& event_id,
                        const std::string& event_code) -> void {
        SeedSoccerEventKind(event_code, event_code);
        sst::db::MatchEvent ev;
        ev.id = event_id;
        ev.match_id = match_id;
        ev.sport_id = seeded_sport_id_;
        ev.event_code = event_code;
        ev.period = 1;
        ev.timestamp_in_match = 1.0;
        ev.wall_clock_at = "2026-05-01T20:00:01";
        ASSERT_TRUE(mgr_->matches().saveEvent(ev).success);
    }

    std::string db_path_;
    std::filesystem::path video_root_;
    std::unique_ptr<sst::db::DbManager> mgr_;

    std::unique_ptr<FakeEncoderPipeline> encoder_owner_;
    std::unique_ptr<FakeSegmentRecorder> segment_owner_;
    std::unique_ptr<FakeEventClipRecorder> event_clip_owner_;
    FakeEncoderPipeline* encoder_{nullptr};
    FakeSegmentRecorder* segment_recorder_{nullptr};
    FakeEventClipRecorder* event_clip_recorder_{nullptr};
    FakeDiskGuard disk_guard_;
    std::unique_ptr<RecordingService> service_;
    int64_t seeded_sport_id_{0};
};

// ── State machine ─────────────────────────────────────────────────────────

TEST_F(RecordingServiceTest, StartFullGameTransitionsToRecording) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa1";
    SeedMatch(match_id);

    EXPECT_EQ(service_->CurrentState(), RecordingState::kIdle);

    auto res = service_->StartFullGame(match_id);
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.recording_id.empty());

    EXPECT_EQ(service_->CurrentState(), RecordingState::kRecording);
    EXPECT_EQ(encoder_->start_calls, 1);
    EXPECT_EQ(segment_recorder_->start_calls, 1);

    EXPECT_EQ(segment_recorder_->last_output_dir,
              video_root_ / match_id / "full_game");
    EXPECT_TRUE(std::filesystem::is_directory(segment_recorder_->last_output_dir));

    auto persisted = mgr_->recordings().get(res.recording_id);
    ASSERT_TRUE(persisted.success);
    EXPECT_EQ(persisted.data.match_id.value_or(""), match_id);
    EXPECT_EQ(persisted.data.kind, sst::db::RecordingKind::kFullGame);
    EXPECT_FALSE(persisted.data.ended_at.has_value());
}

TEST_F(RecordingServiceTest, DoubleStartFullGameRejected) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa2";
    SeedMatch(match_id);

    ASSERT_TRUE(service_->StartFullGame(match_id).success);
    EXPECT_FALSE(service_->StartFullGame(match_id).success);
    EXPECT_EQ(encoder_->start_calls, 1);
    EXPECT_EQ(segment_recorder_->start_calls, 1);
}

TEST_F(RecordingServiceTest, PauseResumeCycle) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa3";
    SeedMatch(match_id);
    ASSERT_TRUE(service_->StartFullGame(match_id).success);

    EXPECT_TRUE(service_->Pause());
    EXPECT_EQ(service_->CurrentState(), RecordingState::kPaused);
    EXPECT_EQ(segment_recorder_->pause_calls, 1);
    EXPECT_FALSE(service_->Pause());  // already paused

    EXPECT_TRUE(service_->Resume());
    EXPECT_EQ(service_->CurrentState(), RecordingState::kRecording);
    EXPECT_EQ(segment_recorder_->resume_calls, 1);
    EXPECT_FALSE(service_->Resume());  // already recording
}

TEST_F(RecordingServiceTest, PauseFromIdleRejected) {
    EXPECT_FALSE(service_->Pause());
    EXPECT_EQ(segment_recorder_->pause_calls, 0);
    EXPECT_EQ(service_->CurrentState(), RecordingState::kIdle);
}

TEST_F(RecordingServiceTest, StopFullGameFinalizes) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa4";
    SeedMatch(match_id);
    auto start = service_->StartFullGame(match_id);
    ASSERT_TRUE(start.success);

    auto stop = service_->StopFullGame();
    EXPECT_TRUE(stop.success);
    EXPECT_FALSE(stop.merged_path.empty());

    EXPECT_EQ(service_->CurrentState(), RecordingState::kIdle);
    EXPECT_EQ(segment_recorder_->stop_calls, 1);
    EXPECT_EQ(encoder_->stop_calls, 1);

    auto reloaded = mgr_->recordings().get(start.recording_id);
    ASSERT_TRUE(reloaded.success);
    EXPECT_TRUE(reloaded.data.ended_at.has_value());

    auto segments = mgr_->recordings().listSegments(start.recording_id);
    ASSERT_TRUE(segments.success);
    EXPECT_EQ(segments.data.size(), 2);
    EXPECT_EQ(segments.data[0].sequence, 0);
    EXPECT_EQ(segments.data[1].sequence, 1);
}

TEST_F(RecordingServiceTest, StopFromIdleRejected) {
    auto stop = service_->StopFullGame();
    EXPECT_FALSE(stop.success);
    EXPECT_EQ(segment_recorder_->stop_calls, 0);
}

TEST_F(RecordingServiceTest, TriggerEventClipCreatesRowsAndCallsRecorder) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa5";
    SeedMatch(match_id);
    const std::string match_event_id = "00000000-0000-4000-8000-bbbbbbbbbbb1";
    SeedMatchEvent(match_id, match_event_id, "goal");
    ASSERT_TRUE(service_->StartFullGame(match_id).success);

    const EventClipWindow window{.pre_seconds = 30, .post_seconds = 45};
    auto res = service_->TriggerEventClip(match_event_id, window);
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.event_clip_id.empty());
    EXPECT_FALSE(res.recording_id.empty());

    EXPECT_EQ(event_clip_recorder_->trigger_calls, 1);
    EXPECT_EQ(event_clip_recorder_->last_window.pre_seconds, 30);
    EXPECT_EQ(event_clip_recorder_->last_window.post_seconds, 45);
    EXPECT_EQ(event_clip_recorder_->last_output_path,
              video_root_ / match_id / ("event_" + res.event_clip_id) / "clip.mp4");
    EXPECT_TRUE(std::filesystem::is_directory(event_clip_recorder_->last_output_path.parent_path()));

    auto persisted_clip = mgr_->recordings().getEventClip(res.event_clip_id);
    ASSERT_TRUE(persisted_clip.success);
    EXPECT_EQ(persisted_clip.data.match_event_id, match_event_id);
    EXPECT_EQ(persisted_clip.data.pre_seconds, 30);
    EXPECT_EQ(persisted_clip.data.post_seconds, 45);
}

TEST_F(RecordingServiceTest, TriggerEventClipFromIdleRejected) {
    auto res = service_->TriggerEventClip("any-event-id", {.pre_seconds = 30, .post_seconds = 30});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(event_clip_recorder_->trigger_calls, 0);
}

TEST_F(RecordingServiceTest, TriggerEventClipDuringPauseSucceeds) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa6";
    SeedMatch(match_id);
    const std::string match_event_id = "00000000-0000-4000-8000-bbbbbbbbbbb2";
    SeedMatchEvent(match_id, match_event_id, "goal");
    ASSERT_TRUE(service_->StartFullGame(match_id).success);
    ASSERT_TRUE(service_->Pause());

    auto res = service_->TriggerEventClip(match_event_id,
                                          {.pre_seconds = 60, .post_seconds = 60});
    EXPECT_TRUE(res.success);
    EXPECT_EQ(event_clip_recorder_->trigger_calls, 1);
    // The encoder ring keeps running across pause; OnEncoded calls are an
    // implementation detail of the recorder doubles, but we did not touch the
    // segment recorder during the trigger.
    EXPECT_EQ(segment_recorder_->pause_calls, 1);
}

TEST_F(RecordingServiceTest, TriggerEventClipRejectedWithBadWindow) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa7";
    SeedMatch(match_id);
    ASSERT_TRUE(service_->StartFullGame(match_id).success);

    auto bad_pre = service_->TriggerEventClip("evt", {.pre_seconds = 0, .post_seconds = 30});
    EXPECT_FALSE(bad_pre.success);
    auto bad_post = service_->TriggerEventClip("evt", {.pre_seconds = 30, .post_seconds = -1});
    EXPECT_FALSE(bad_post.success);
    EXPECT_EQ(event_clip_recorder_->trigger_calls, 0);
}

TEST_F(RecordingServiceTest, PushDelegatesToEncoderWhileNonIdle) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa8";
    SeedMatch(match_id);

    sst::capture::Frame frame;
    frame.format = sst::common::PixelFormat::BGR8;
    frame.geometry = {.width = 1920, .height = 1080};

    service_->Push(frame);
    EXPECT_EQ(encoder_->push_calls, 0);

    ASSERT_TRUE(service_->StartFullGame(match_id).success);
    service_->Push(frame);
    EXPECT_EQ(encoder_->push_calls, 1);

    ASSERT_TRUE(service_->Pause());
    service_->Push(frame);
    EXPECT_EQ(encoder_->push_calls, 2);  // still pushed during pause

    ASSERT_TRUE(service_->StopFullGame().success);
    service_->Push(frame);
    EXPECT_EQ(encoder_->push_calls, 2);  // not pushed after stop
}

// ── Disk guard ─────────────────────────────────────────────────────────

TEST_F(RecordingServiceTest, StartFullGameRejectedWhenDiskGuardRefuses) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaa9";
    SeedMatch(match_id);

    disk_guard_.has_space = false;
    disk_guard_.free_bytes = 100;

    auto res = service_->StartFullGame(match_id);
    EXPECT_FALSE(res.success);
    EXPECT_TRUE(res.recording_id.empty());

    // No side effects: encoder stays off, no DB row inserted.
    EXPECT_EQ(encoder_->start_calls, 0);
    EXPECT_EQ(segment_recorder_->start_calls, 0);
    auto persisted = mgr_->recordings().listByMatch(match_id);
    ASSERT_TRUE(persisted.success);
    EXPECT_TRUE(persisted.data.empty());
    EXPECT_EQ(service_->CurrentState(), RecordingState::kIdle);
}

TEST_F(RecordingServiceTest, TriggerEventClipRejectedWhenDiskGuardRefusesMidGame) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaaba";
    SeedMatch(match_id);
    const std::string match_event_id = "00000000-0000-4000-8000-bbbbbbbbbbb3";
    SeedMatchEvent(match_id, match_event_id, "goal");

    // Start with the disk fine, then simulate the SSD filling up mid-game.
    ASSERT_TRUE(service_->StartFullGame(match_id).success);
    disk_guard_.has_space = false;

    auto res = service_->TriggerEventClip(match_event_id,
                                          {.pre_seconds = 60, .post_seconds = 60});
    EXPECT_FALSE(res.success);
    EXPECT_EQ(event_clip_recorder_->trigger_calls, 0);

    // The full-game recording is unaffected — still active, no rollback.
    EXPECT_EQ(service_->CurrentState(), RecordingState::kRecording);
}

// ── Filesystem rollback ───────────────────────────────────────────────

TEST_F(RecordingServiceTest, StartFullGameCleansUpFilesystemWhenEncoderRefuses) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaabb";
    SeedMatch(match_id);
    encoder_->accept_start = false;

    auto res = service_->StartFullGame(match_id);
    EXPECT_FALSE(res.success);

    // No directories left behind from the failed attempt — neither the
    // full_game subdir nor the parent <match_uuid> dir.
    const auto match_dir = video_root_ / match_id;
    EXPECT_FALSE(std::filesystem::exists(match_dir));

    // No DB row either.
    auto rows = mgr_->recordings().listByMatch(match_id);
    ASSERT_TRUE(rows.success);
    EXPECT_TRUE(rows.data.empty());
}

TEST_F(RecordingServiceTest, StartFullGameCleansUpFilesystemWhenSegmentRecorderRefuses) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaabc";
    SeedMatch(match_id);
    segment_recorder_->accept_start = false;

    auto res = service_->StartFullGame(match_id);
    EXPECT_FALSE(res.success);

    EXPECT_FALSE(std::filesystem::exists(video_root_ / match_id));
    // Encoder was started then stopped because segment recorder refused.
    EXPECT_EQ(encoder_->start_calls, 1);
    EXPECT_EQ(encoder_->stop_calls, 1);
    auto rows = mgr_->recordings().listByMatch(match_id);
    ASSERT_TRUE(rows.success);
    EXPECT_TRUE(rows.data.empty());
}

TEST_F(RecordingServiceTest, TriggerEventClipCleansUpFilesystemWhenRecorderRefuses) {
    const std::string match_id = "00000000-0000-4000-8000-aaaaaaaaaabd";
    SeedMatch(match_id);
    const std::string match_event_id = "00000000-0000-4000-8000-bbbbbbbbbbb4";
    SeedMatchEvent(match_id, match_event_id, "goal");

    auto start = service_->StartFullGame(match_id);
    ASSERT_TRUE(start.success);

    // The full-game recording is in progress, so the match dir must exist.
    ASSERT_TRUE(std::filesystem::exists(video_root_ / match_id / "full_game"));

    event_clip_recorder_->accept_next_trigger = false;
    auto res = service_->TriggerEventClip(match_event_id,
                                          {.pre_seconds = 60, .post_seconds = 60});
    EXPECT_FALSE(res.success);

    // No event_<uuid> dir survived the rollback. The full_game dir is
    // untouched — the in-progress recording owns it.
    EXPECT_TRUE(std::filesystem::exists(video_root_ / match_id / "full_game"));
    int event_dirs = 0;
    for (const auto& entry : std::filesystem::directory_iterator(video_root_ / match_id)) {
        if (entry.path().filename().string().rfind("event_", 0) == 0) {
            ++event_dirs;
        }
    }
    EXPECT_EQ(event_dirs, 0);

    // The active full-game recording is unaffected.
    EXPECT_EQ(service_->CurrentState(), RecordingState::kRecording);
}

}  // namespace
