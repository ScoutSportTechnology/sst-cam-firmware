#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/control/services/controllers/camera.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"
#include "domain/db/models/camera.hpp"

namespace {

using sst::control::CameraController;
using sst::control::Command;
using sst::control::CommandStatus;

constexpr std::int64_t kDefaultUserId = 1;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

auto MakeCommand(std::string_view text) -> Command {
    Command cmd;
    cmd.route = "camera";
    cmd.payload.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(text[i]);
    }
    return cmd;
}

auto PayloadToString(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class CameraControllerTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/control/data/camera_" + suffix + ".db"))
                       .string();
        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });
        ASSERT_TRUE(mgr_->users().create("test").success);
        // Seed a default config so get_config has something to return.
        sst::db::CameraConfig seeded;
        seeded.user_id = kDefaultUserId;
        ASSERT_TRUE(mgr_->cameras().saveConfig(seeded).success);

        controller_ = std::make_unique<CameraController>(mgr_->cameras(), kDefaultUserId);
    }

    auto TearDown() -> void override {
        controller_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    std::string db_path_;
    std::unique_ptr<sst::db::DbManager> mgr_;
    std::unique_ptr<CameraController> controller_;
};

TEST_F(CameraControllerTest, GetConfigReturnsAllFields) {
    auto res = controller_->Handle(MakeCommand("get_config"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));

    EXPECT_EQ(j["user_id"].get<std::int64_t>(), kDefaultUserId);
    EXPECT_EQ(j["exposure"].get<std::int32_t>(), sst::db::CameraConfig::kDefaultExposure);
    EXPECT_EQ(j["fps"].get<std::int32_t>(), sst::db::CameraConfig::kDefaultFps);
    EXPECT_EQ(j["event_clip_pre_seconds"].get<std::int32_t>(),
              sst::db::CameraConfig::kDefaultEventClipPreSeconds);
    EXPECT_EQ(j["event_clip_post_seconds"].get<std::int32_t>(),
              sst::db::CameraConfig::kDefaultEventClipPostSeconds);
}

TEST_F(CameraControllerTest, SetConfigPartialUpdatePersistsAndLeavesOtherFieldsAlone) {
    auto res = controller_->Handle(
        MakeCommand(R"(set_config {"exposure":250,"event_clip_pre_seconds":45})"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    EXPECT_EQ(j["exposure"].get<std::int32_t>(), 250);
    EXPECT_EQ(j["event_clip_pre_seconds"].get<std::int32_t>(), 45);
    // Untouched field still has its default.
    EXPECT_EQ(j["fps"].get<std::int32_t>(), sst::db::CameraConfig::kDefaultFps);

    auto reloaded = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.exposure, 250);
    EXPECT_EQ(reloaded.data.event_clip_pre_seconds, 45);
    EXPECT_EQ(reloaded.data.event_clip_post_seconds,
              sst::db::CameraConfig::kDefaultEventClipPostSeconds);
}

TEST_F(CameraControllerTest, SetConfigAcceptsAllFields) {
    auto res = controller_->Handle(MakeCommand(
        R"(set_config {"exposure":150,"gain":2.5,"white_balance":1,"focus":1,)"
        R"("width":1280,"height":720,"format":3,"fps":30,)"
        R"("event_clip_pre_seconds":30,"event_clip_post_seconds":75})"));
    ASSERT_EQ(res.status, CommandStatus::kOk);

    auto reloaded = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(reloaded.success);
    EXPECT_EQ(reloaded.data.exposure, 150);
    EXPECT_FLOAT_EQ(reloaded.data.gain, 2.5F);
    EXPECT_EQ(reloaded.data.white_balance, sst::db::CameraWhiteBalance::kManual);
    EXPECT_EQ(reloaded.data.focus, sst::db::CameraFocus::kManual);
    EXPECT_EQ(reloaded.data.width, 1280);
    EXPECT_EQ(reloaded.data.height, 720);
    EXPECT_EQ(reloaded.data.fps, 30);
    EXPECT_EQ(reloaded.data.event_clip_pre_seconds, 30);
    EXPECT_EQ(reloaded.data.event_clip_post_seconds, 75);
}

TEST_F(CameraControllerTest, SetConfigRejectsNonObjectPayload) {
    EXPECT_EQ(controller_->Handle(MakeCommand("set_config 123")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config "string")")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(controller_->Handle(MakeCommand("set_config not-json")).status,
              CommandStatus::kInvalidArgument);
}

TEST_F(CameraControllerTest, SetConfigRejectsNonPositiveNumbers) {
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config {"exposure":0})")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config {"gain":0})")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config {"fps":-1})")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(
        controller_->Handle(MakeCommand(R"(set_config {"event_clip_pre_seconds":0})")).status,
        CommandStatus::kInvalidArgument);
}

TEST_F(CameraControllerTest, SetConfigRejectsWrongTypes) {
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config {"exposure":"high"})")).status,
              CommandStatus::kInvalidArgument);
    EXPECT_EQ(controller_->Handle(MakeCommand(R"(set_config {"width":"big"})")).status,
              CommandStatus::kInvalidArgument);
}

TEST_F(CameraControllerTest, SetConfigRejectionLeavesDbUnchanged) {
    auto before = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(before.success);
    const auto exposure_before = before.data.exposure;

    auto res = controller_->Handle(MakeCommand(R"(set_config {"exposure":-99})"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);

    auto after = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(after.success);
    EXPECT_EQ(after.data.exposure, exposure_before);
}

TEST_F(CameraControllerTest, GetConfigReturnsNotFoundForMissingRow) {
    // Fresh user, no camera_config row inserted for user 999.
    sst::control::CameraController other_user_controller(mgr_->cameras(), /*user_id=*/999);
    EXPECT_EQ(other_user_controller.Handle(MakeCommand("get_config")).status,
              CommandStatus::kNotFound);
}

TEST_F(CameraControllerTest, UnknownVerbRejected) {
    EXPECT_EQ(controller_->Handle(MakeCommand("blow_dryer")).status,
              CommandStatus::kInvalidArgument);
}

}  // namespace
