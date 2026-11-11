#include "app/control/cli.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace sst::control::test {

TEST(CliTest, NoArgsRunsFirmware) {
    std::vector<const char*> args = {"sst_cam_firmware"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kRunFirmware);
}

TEST(CliTest, VersionFlagPrintsVersionAndExits) {
    std::vector<const char*> args = {"sst_cam_firmware", "--version"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kPrintVersion);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.output, "sst_cam_firmware v0.1.0 (git abc1234)");
}

TEST(CliTest, HelpShortFlagPrintsHelpAndExits) {
    std::vector<const char*> args = {"sst_cam_firmware", "-h"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kPrintHelp);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.output.find("Usage:") != std::string::npos);
    EXPECT_TRUE(result.output.find("--version") != std::string::npos);
    EXPECT_TRUE(result.output.find("--help") != std::string::npos);
}

TEST(CliTest, HelpLongFlagPrintsHelpAndExits) {
    std::vector<const char*> args = {"sst_cam_firmware", "--help"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kPrintHelp);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.output.find("Usage:") != std::string::npos);
}

TEST(CliTest, UnknownFlagPrintsErrorAndExits2) {
    std::vector<const char*> args = {"sst_cam_firmware", "--unknown"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kError);
    EXPECT_EQ(result.exit_code, 2);
    EXPECT_TRUE(result.error.find("unrecognized option") != std::string::npos);
    EXPECT_TRUE(result.error.find("--unknown") != std::string::npos);
}

TEST(CliTest, MultipleArgsFirstUnknownWins) {
    std::vector<const char*> args = {"sst_cam_firmware", "--unknown", "--version"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "v0.1.0", "abc1234");
    EXPECT_EQ(result.action, CliAction::kError);
    EXPECT_EQ(result.exit_code, 2);
    EXPECT_TRUE(result.error.find("unrecognized option") != std::string::npos);
}

TEST(CliTest, VersionWithDevVersion) {
    std::vector<const char*> args = {"sst_cam_firmware", "--version"};
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const auto* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    auto result = ParseArgs(static_cast<int>(argv.size()), argv.data(), "dev", "unknown");
    EXPECT_EQ(result.action, CliAction::kPrintVersion);
    EXPECT_EQ(result.output, "sst_cam_firmware dev (git unknown)");
}

}  // namespace sst::control::test