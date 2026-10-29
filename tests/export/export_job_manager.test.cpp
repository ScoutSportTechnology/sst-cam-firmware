// Unit tests for the overlay-burn job manager (#6 F6c). The burner + path/token
// collaborators are fakes, so no real video files are decoded here — the burn
// itself is a hardware-bound test validated on-device.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

#include "app/export/services/export-job-manager.hpp"
#include "app/network/services/download_server/download-server.hpp"
#include "app/overlay/ports/overlay-burner.hpp"
#include "domain/network/models/download-token.hpp"
#include "domain/overlay/models/overlay-timeline.hpp"

namespace fs = std::filesystem;

namespace {

constexpr int kMaxPolls = 200;
constexpr int kPollSleepMs = 5;
constexpr std::uint64_t kStartTime = 100;
constexpr std::uint64_t kTokenTtl = 60;
constexpr int kEvictionCount = 16;

// Configurable fake burn — records the call and returns a preset result.
class FakeBurner final : public sst::overlay::IOverlayBurner {
   public:
    auto Burn(const fs::path& l1_path, const sst::overlay::OverlayTimeline& /*timeline*/,
              const fs::path& l2_path, const std::atomic<bool>& /*cancel*/) -> bool override {
        ++calls;
        last_l1 = l1_path;
        last_l2 = l2_path;
        if (write_file && result) {
            std::ofstream(l2_path) << "burned";
        }
        return result;
    }
    std::atomic<int> calls{0};
    bool result{true};
    bool write_file{false};
    fs::path last_l1;
    fs::path last_l2;
};

auto MakeToken() -> sst::network::DownloadToken {
    sst::network::DownloadToken token;
    token.token = "tok-abc";
    token.expires_at_unix = 1;
    return token;
}

// Poll until the job leaves pending/running (the worker is async). Fakes are
// instant, so this resolves within a couple of iterations.
auto WaitForTerminal(const sst::exportjob::ExportJobManager& manager,
                     const std::string& job_id) -> sst::exportjob::ExportJobView {
    for (int i = 0; i < kMaxPolls; ++i) {
        const auto view = manager.Poll(job_id);
        if (view && (view->state == sst::exportjob::ExportState::kReady ||
                     view->state == sst::exportjob::ExportState::kFailed)) {
            return *view;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollSleepMs));
    }
    return sst::exportjob::ExportJobView{};
}

}  // namespace

TEST(ExportJobManagerTest, ReadyWithTokenOnSuccess) {
    FakeBurner burner;
    sst::exportjob::ExportJobManager manager(
        burner, [](const std::string&) { return std::optional<fs::path>{"/v/match.mp4"}; },
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [](const fs::path&, const std::string&) { return std::optional{MakeToken()}; });

    const auto job_id = manager.RequestExport("match");
    const auto view = WaitForTerminal(manager, job_id);

    EXPECT_EQ(view.state, sst::exportjob::ExportState::kReady);
    ASSERT_TRUE(view.token.has_value());
    if (view.token) {  // narrows for clang-tidy (it can't see through ASSERT_TRUE)
        EXPECT_EQ(view.token->token, "tok-abc");
    }
    EXPECT_EQ(burner.calls.load(), 1);
}

TEST(ExportJobManagerTest, BurnedRecordingIsCatalogedAndSurvivesJobEviction) {
    const fs::path root =
        fs::temp_directory_path() /
        ("sst-export-persistent-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path match_dir = root / "user" / "match-a";
    fs::create_directories(match_dir);
    std::ofstream(match_dir / "match-a.mp4") << "clean";
    std::uint64_t now = kStartTime;
    sst::network::DownloadServer downloads(root, root, [&now] { return now; });
    FakeBurner burner;
    burner.write_file = true;
    sst::exportjob::ExportJobManager manager(
        burner,
        [&downloads](const std::string& recording_id) {
            return downloads.ResolveRecordingPath(recording_id);
        },
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [&downloads](const fs::path&, const std::string& recording_id) {
            return downloads.MintToken(recording_id, kTokenTtl);
        });

    const auto first = WaitForTerminal(manager, manager.RequestExport("match-a"));
    ASSERT_EQ(first.state, sst::exportjob::ExportState::kReady);
    ASSERT_TRUE(first.token);
    if (first.token) {
        EXPECT_EQ(first.token->recording_id, "match-a_overlay");
        EXPECT_EQ(downloads.ValidateToken(first.token->token), burner.last_l2);
        EXPECT_EQ(downloads.ValidateToken(first.token->token), burner.last_l2);
        now = kStartTime + kTokenTtl;
        EXPECT_FALSE(downloads.ValidateToken(first.token->token));
    }
    EXPECT_EQ(burner.last_l2, match_dir / "match-a_overlay.mp4");
    EXPECT_EQ(downloads.Enumerate().size(), 2U);
    EXPECT_EQ(downloads.ResolveRecordingPath("match-a_overlay"), burner.last_l2);
    const auto renewed = downloads.MintToken("match-a_overlay", kTokenTtl);
    ASSERT_TRUE(renewed);
    if (renewed) {
        EXPECT_EQ(renewed->recording_id, "match-a_overlay");
    }

    for (int i = 0; i < kEvictionCount; ++i) {
        const std::string recording_id = "match-" + std::to_string(i);
        const fs::path dir = root / "user" / recording_id;
        fs::create_directories(dir);
        std::ofstream(dir / (recording_id + ".mp4")) << "clean";
        EXPECT_EQ(WaitForTerminal(manager, manager.RequestExport(recording_id)).state,
                  sst::exportjob::ExportState::kReady);
    }
    EXPECT_TRUE(fs::exists(match_dir / "match-a_overlay.mp4"));
    {
        FakeBurner restarted_burner;
        sst::exportjob::ExportJobManager restarted(
            restarted_burner,
            [&downloads](const std::string& recording_id) {
                return downloads.ResolveRecordingPath(recording_id);
            },
            [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
            [&downloads](const fs::path&, const std::string& recording_id) {
                return downloads.MintToken(recording_id, kTokenTtl);
            });
        const auto reused = WaitForTerminal(restarted, restarted.RequestExport("match-a"));
        EXPECT_EQ(reused.state, sst::exportjob::ExportState::kReady);
        EXPECT_EQ(restarted_burner.calls.load(), 0);
    }
    fs::remove_all(root);
}

TEST(ExportJobManagerTest, RefusesSymlinkAtStableExportPath) {
    const fs::path root =
        fs::temp_directory_path() /
        ("sst-export-symlink-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    std::ofstream(root / "match.mp4") << "clean";
    std::ofstream(root / "other.mp4") << "other";
    fs::create_symlink(root / "other.mp4", root / "match_overlay.mp4");
    FakeBurner burner;
    sst::network::DownloadServer downloads(root, root, [] { return kStartTime; });
    sst::exportjob::ExportJobManager manager(
        burner,
        [&downloads](const std::string& recording_id) {
            return downloads.ResolveRecordingPath(recording_id);
        },
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [&downloads](const fs::path&, const std::string& recording_id) {
            return downloads.MintToken(recording_id, kTokenTtl);
        });

    const auto view = WaitForTerminal(manager, manager.RequestExport("match"));
    EXPECT_EQ(view.state, sst::exportjob::ExportState::kFailed);
    EXPECT_EQ(burner.calls.load(), 0);
    fs::remove_all(root);
}

TEST(ExportJobManagerTest, FailsWhenRecordingNotFound) {
    FakeBurner burner;
    sst::exportjob::ExportJobManager manager(
        burner, [](const std::string&) { return std::optional<fs::path>{}; },  // unresolved
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [](const fs::path&, const std::string&) { return std::optional{MakeToken()}; });

    const auto view = WaitForTerminal(manager, manager.RequestExport("ghost"));

    EXPECT_EQ(view.state, sst::exportjob::ExportState::kFailed);
    EXPECT_EQ(burner.calls.load(), 0);  // never reached the burn
    EXPECT_FALSE(view.error.empty());
}

TEST(ExportJobManagerTest, FailsWhenBurnFails) {
    FakeBurner burner;
    burner.result = false;
    sst::exportjob::ExportJobManager manager(
        burner, [](const std::string&) { return std::optional<fs::path>{"/v/match.mp4"}; },
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [](const fs::path&, const std::string&) { return std::optional{MakeToken()}; });

    const auto view = WaitForTerminal(manager, manager.RequestExport("match"));

    EXPECT_EQ(view.state, sst::exportjob::ExportState::kFailed);
    EXPECT_EQ(burner.calls.load(), 1);
    EXPECT_FALSE(view.token.has_value());
}

TEST(ExportJobManagerTest, PollUnknownJobIsNullopt) {
    FakeBurner burner;
    sst::exportjob::ExportJobManager manager(
        burner, [](const std::string&) { return std::optional<fs::path>{"/v/match.mp4"}; },
        [](const fs::path&) { return sst::overlay::OverlayTimeline{}; },
        [](const fs::path&, const std::string&) { return std::optional{MakeToken()}; });

    EXPECT_FALSE(manager.Poll("export-999").has_value());
}
