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

#include "app/control/services/controllers/team.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "domain/control/models/command-result.hpp"
#include "domain/control/models/command.hpp"

namespace {

using sst::control::Command;
using sst::control::CommandResult;
using sst::control::CommandStatus;
using sst::control::TeamController;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

auto MakeCommand(std::string_view text) -> Command {
    Command cmd;
    cmd.route = "team";
    cmd.payload.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(text[i]);
    }
    return cmd;
}

auto PayloadToString(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class TeamControllerTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        db_path_ = (std::filesystem::path(SST_REPO_ROOT_DIR) /
                    ("tests/control/data/team_" + suffix + ".db"))
                       .string();
        std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());
        std::filesystem::remove(db_path_);

        mgr_ = std::make_unique<sst::db::DbManager>(sst::db::DbManager::Config{
            .db_path = db_path_,
            .schema_path = SST_REPO_ROOT_DIR "/db/schema.sql",
        });
        controller_ = std::make_unique<TeamController>(mgr_->teams());

        // A sport for the team FK to point at.
        auto sport =
            mgr_->sports().save({.code = "soccer", .name = "Soccer", .periods = 2});
        soccer_id_ = sport.data.id;
        ASSERT_GT(soccer_id_, 0);
    }

    auto TearDown() -> void override {
        controller_.reset();
        mgr_.reset();
        std::error_code ec;
        std::filesystem::remove(db_path_, ec);
    }

    std::string db_path_;
    std::unique_ptr<sst::db::DbManager> mgr_;
    std::unique_ptr<TeamController> controller_;
    std::int64_t soccer_id_{0};
};

TEST_F(TeamControllerTest, CreateInsertsRow) {
    auto res = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    EXPECT_EQ(res.status, CommandStatus::kOk);
    const auto id = std::stoll(PayloadToString(res.payload));

    auto row = mgr_->teams().get(id);
    ASSERT_TRUE(row.success);
    EXPECT_EQ(row.data.sport_id, soccer_id_);
    EXPECT_EQ(row.data.name, "Alpha");
}

TEST_F(TeamControllerTest, CreateRejectsMissingFields) {
    auto res = controller_->Handle(
        MakeCommand(R"(create {"name":"Alpha","short_name":"A"})"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(TeamControllerTest, CreateRejectsUnknownSportId) {
    auto res = controller_->Handle(
        MakeCommand(R"(create {"sport_id":9999,"name":"Alpha","short_name":"A"})"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

TEST_F(TeamControllerTest, CreateRejectsDuplicateNamePerSport) {
    auto first = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(first.status, CommandStatus::kOk);

    auto dup = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A2"}})", soccer_id_)));
    EXPECT_EQ(dup.status, CommandStatus::kInvalidArgument);
}

TEST_F(TeamControllerTest, ListReturnsTeamsForSport) {
    ASSERT_EQ(
        controller_
            ->Handle(MakeCommand(fmt::format(
                R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)))
            .status,
        CommandStatus::kOk);
    ASSERT_EQ(
        controller_
            ->Handle(MakeCommand(fmt::format(
                R"(create {{"sport_id":{},"name":"Bravo","short_name":"B"}})", soccer_id_)))
            .status,
        CommandStatus::kOk);

    auto res = controller_->Handle(MakeCommand(fmt::format("list {}", soccer_id_)));
    EXPECT_EQ(res.status, CommandStatus::kOk);
    const auto body = PayloadToString(res.payload);
    EXPECT_NE(body.find("Alpha A"), std::string::npos);
    EXPECT_NE(body.find("Bravo B"), std::string::npos);
}

TEST_F(TeamControllerTest, DeleteRemovesRow) {
    auto created = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto id = PayloadToString(created.payload);

    auto del = controller_->Handle(MakeCommand("delete " + id));
    EXPECT_EQ(del.status, CommandStatus::kOk);

    EXPECT_FALSE(mgr_->teams().get(std::stoll(id)).success);
}

TEST_F(TeamControllerTest, DeleteUnknownIdReturnsNotFound) {
    auto del = controller_->Handle(MakeCommand("delete 9999"));
    EXPECT_EQ(del.status, CommandStatus::kNotFound);
}

TEST_F(TeamControllerTest, AddPlayerWithJerseyRoundTrips) {
    auto created = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto team_id = std::stoll(PayloadToString(created.payload));

    auto add = controller_->Handle(MakeCommand(fmt::format(
        R"(add_player {{"team_id":{},"name":"Striker","jersey_number":9}})", team_id)));
    EXPECT_EQ(add.status, CommandStatus::kOk);

    auto list = controller_->Handle(MakeCommand(fmt::format("list_players {}", team_id)));
    EXPECT_EQ(list.status, CommandStatus::kOk);
    const auto body = PayloadToString(list.payload);
    EXPECT_NE(body.find("Striker 9"), std::string::npos);
}

TEST_F(TeamControllerTest, AddPlayerWithoutJerseyShowsDashInList) {
    auto created = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto team_id = std::stoll(PayloadToString(created.payload));

    auto add = controller_->Handle(MakeCommand(fmt::format(
        R"(add_player {{"team_id":{},"name":"Reserve"}})", team_id)));
    EXPECT_EQ(add.status, CommandStatus::kOk);

    auto list = controller_->Handle(MakeCommand(fmt::format("list_players {}", team_id)));
    EXPECT_EQ(list.status, CommandStatus::kOk);
    const auto body = PayloadToString(list.payload);
    EXPECT_NE(body.find("Reserve -"), std::string::npos);
}

TEST_F(TeamControllerTest, AddPlayerRejectsUnknownTeam) {
    auto add = controller_->Handle(
        MakeCommand(R"(add_player {"team_id":9999,"name":"Ghost"})"));
    EXPECT_EQ(add.status, CommandStatus::kInvalidArgument);
}

TEST_F(TeamControllerTest, AddPlayerRejectsDuplicateJersey) {
    auto created = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto team_id = std::stoll(PayloadToString(created.payload));

    ASSERT_EQ(controller_
                  ->Handle(MakeCommand(fmt::format(
                      R"(add_player {{"team_id":{},"name":"First","jersey_number":9}})",
                      team_id)))
                  .status,
              CommandStatus::kOk);
    auto dup = controller_->Handle(MakeCommand(fmt::format(
        R"(add_player {{"team_id":{},"name":"Second","jersey_number":9}})", team_id)));
    EXPECT_EQ(dup.status, CommandStatus::kInvalidArgument);
}

TEST_F(TeamControllerTest, RemovePlayerRemovesRow) {
    auto created = controller_->Handle(MakeCommand(fmt::format(
        R"(create {{"sport_id":{},"name":"Alpha","short_name":"A"}})", soccer_id_)));
    ASSERT_EQ(created.status, CommandStatus::kOk);
    const auto team_id = std::stoll(PayloadToString(created.payload));

    auto add = controller_->Handle(MakeCommand(fmt::format(
        R"(add_player {{"team_id":{},"name":"Striker","jersey_number":9}})", team_id)));
    ASSERT_EQ(add.status, CommandStatus::kOk);
    const auto player_id = PayloadToString(add.payload);

    auto remove = controller_->Handle(MakeCommand("remove_player " + player_id));
    EXPECT_EQ(remove.status, CommandStatus::kOk);

    auto remove_again = controller_->Handle(MakeCommand("remove_player " + player_id));
    EXPECT_EQ(remove_again.status, CommandStatus::kNotFound);
}

TEST_F(TeamControllerTest, UnknownVerbRejected) {
    auto res = controller_->Handle(MakeCommand("yeet"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

}  // namespace
