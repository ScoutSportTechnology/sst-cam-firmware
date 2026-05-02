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

#include "app/control/services/controllers/sport.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"

namespace {

using sst::control::Command;
using sst::control::CommandResult;
using sst::control::CommandStatus;
using sst::control::SportController;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

auto MakeCommand(std::string_view text) -> Command {
    Command cmd;
    cmd.route = "sport";
    cmd.payload.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(text[i]);
    }
    return cmd;
}

auto PayloadToString(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class SportControllerTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/control/data/sport_" + suffix + ".db"))
                       .string();
        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });
        controller_ = std::make_unique<SportController>(mgr_->sports());
    }

    auto TearDown() -> void override {
        controller_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    std::string db_path_;
    std::unique_ptr<sst::db::DbManager> mgr_;
    std::unique_ptr<SportController> controller_;
};

TEST_F(SportControllerTest, CreateInsertsRowAndReturnsId) {
    auto res = controller_->Handle(
        MakeCommand(R"(create {"code":"soccer","name":"Soccer","periods":2})"));
    EXPECT_EQ(res.status, CommandStatus::kOk);
    EXPECT_FALSE(res.payload.empty());
    const auto id = std::stoll(PayloadToString(res.payload));
    EXPECT_GT(id, 0);

    auto row = mgr_->sports().get(id);
    ASSERT_TRUE(row.success);
    EXPECT_EQ(row.data.code, "soccer");
    EXPECT_EQ(row.data.name, "Soccer");
    EXPECT_EQ(row.data.periods, 2);
}

TEST_F(SportControllerTest, CreateRejectsInvalidJson) {
    auto res = controller_->Handle(MakeCommand("create not-json-at-all"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(SportControllerTest, CreateRejectsMissingFields) {
    auto res = controller_->Handle(MakeCommand(R"(create {"code":"soccer"})"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(SportControllerTest, CreateRejectsZeroPeriods) {
    auto res = controller_->Handle(
        MakeCommand(R"(create {"code":"x","name":"X","periods":0})"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(SportControllerTest, ListReturnsAllRows) {
    ASSERT_EQ(controller_
                  ->Handle(MakeCommand(R"(create {"code":"soccer","name":"Soccer","periods":2})"))
                  .status,
              CommandStatus::kOk);
    ASSERT_EQ(controller_
                  ->Handle(MakeCommand(
                      R"(create {"code":"basketball","name":"Basketball","periods":4})"))
                  .status,
              CommandStatus::kOk);

    auto res = controller_->Handle(MakeCommand("list"));
    EXPECT_EQ(res.status, CommandStatus::kOk);
    const auto body = PayloadToString(res.payload);
    EXPECT_NE(body.find("soccer"), std::string::npos);
    EXPECT_NE(body.find("basketball"), std::string::npos);
    // Two rows = two newlines.
    EXPECT_EQ(std::count(body.begin(), body.end(), '\n'), 2);
}

TEST_F(SportControllerTest, DeleteRemovesRow) {
    auto created = controller_->Handle(
        MakeCommand(R"(create {"code":"soccer","name":"Soccer","periods":2})"));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto id = PayloadToString(created.payload);

    auto del = controller_->Handle(MakeCommand("delete " + id));
    EXPECT_EQ(del.status, CommandStatus::kOk);

    EXPECT_FALSE(mgr_->sports().get(std::stoll(id)).success);
}

TEST_F(SportControllerTest, DeleteUnknownIdReturnsNotFound) {
    auto del = controller_->Handle(MakeCommand("delete 9999"));
    EXPECT_EQ(del.status, CommandStatus::kNotFound);
}

TEST_F(SportControllerTest, DeleteRejectsNonNumericId) {
    auto del = controller_->Handle(MakeCommand("delete abc"));
    EXPECT_EQ(del.status, CommandStatus::kInvalidArgument);
}

TEST_F(SportControllerTest, AddEventKindRoundTripsThroughList) {
    auto created = controller_->Handle(
        MakeCommand(R"(create {"code":"soccer","name":"Soccer","periods":2})"));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto sport_id = std::stoll(PayloadToString(created.payload));

    auto add = controller_->Handle(MakeCommand(fmt::format(
        R"(add_event_kind {{"sport_id":{},"code":"goal","name":"Goal"}})", sport_id)));
    EXPECT_EQ(add.status, CommandStatus::kOk);

    auto add2 = controller_->Handle(MakeCommand(fmt::format(
        R"(add_event_kind {{"sport_id":{},"code":"yellow_card","name":"Yellow Card"}})",
        sport_id)));
    EXPECT_EQ(add2.status, CommandStatus::kOk);

    auto list = controller_->Handle(MakeCommand(fmt::format("list_event_kinds {}", sport_id)));
    EXPECT_EQ(list.status, CommandStatus::kOk);
    const auto body = PayloadToString(list.payload);
    EXPECT_NE(body.find("goal Goal"), std::string::npos);
    EXPECT_NE(body.find("yellow_card Yellow Card"), std::string::npos);
}

TEST_F(SportControllerTest, AddEventKindRejectsUnknownSportId) {
    auto add = controller_->Handle(MakeCommand(
        R"(add_event_kind {"sport_id":9999,"code":"goal","name":"Goal"}))"));
    // FK violation surfaces as kInternal (DbResult::fail). The controller
    // could classify FK as kInvalidArgument but we don't distinguish today.
    EXPECT_NE(add.status, CommandStatus::kOk);
}

TEST_F(SportControllerTest, RemoveEventKindRemovesRow) {
    auto created = controller_->Handle(
        MakeCommand(R"(create {"code":"soccer","name":"Soccer","periods":2})"));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto sport_id = std::stoll(PayloadToString(created.payload));

    ASSERT_EQ(
        controller_
            ->Handle(MakeCommand(fmt::format(
                R"(add_event_kind {{"sport_id":{},"code":"goal","name":"Goal"}})", sport_id)))
            .status,
        CommandStatus::kOk);

    auto remove = controller_->Handle(
        MakeCommand(fmt::format("remove_event_kind {} goal", sport_id)));
    EXPECT_EQ(remove.status, CommandStatus::kOk);

    auto remove_again = controller_->Handle(
        MakeCommand(fmt::format("remove_event_kind {} goal", sport_id)));
    EXPECT_EQ(remove_again.status, CommandStatus::kNotFound);
}

TEST_F(SportControllerTest, UnknownVerbRejected) {
    auto res = controller_->Handle(MakeCommand("punch_yourself"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

}  // namespace
