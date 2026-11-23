#include "app/control/cli.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
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

TEST(CliTest, CaptureDiagnosticRequiresExplicitFlag) {
    std::vector<char*> args = {const_cast<char*>("sst_cam_firmware"),
                               const_cast<char*>("--capture-diagnostic")};
    setenv("SST_ENABLE_ARGUS_DIAGNOSTIC", "1", 1);
    auto result = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    unsetenv("SST_ENABLE_ARGUS_DIAGNOSTIC");
    EXPECT_EQ(result.action, CliAction::kCaptureDiagnostic);
}

TEST(CliTest, CaptureDiagnosticAcceptsOnlyGatedBrightAndDarkStarts) {
    std::vector<char*> args = {const_cast<char*>("sst_cam_firmware"),
                               const_cast<char*>("--capture-diagnostic")};
    setenv("SST_ENABLE_ARGUS_DIAGNOSTIC", "1", 1);
    setenv("SST_ARGUS_DIAGNOSTIC_START", "bright", 1);
    auto bright = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(bright.action, CliAction::kCaptureDiagnostic);
    EXPECT_EQ(bright.diagnostic_seconds, 600);
    setenv("SST_ARGUS_DIAGNOSTIC_START", "dark", 1);
    auto dark = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(dark.action, CliAction::kCaptureDiagnostic);
    setenv("SST_ARGUS_DIAGNOSTIC_SECONDS", "30", 1);
    auto short_run = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(short_run.diagnostic_seconds, 30);
    setenv("SST_ARGUS_DIAGNOSTIC_SECONDS", "invalid", 1);
    auto invalid_duration = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(invalid_duration.action, CliAction::kError);
    unsetenv("SST_ARGUS_DIAGNOSTIC_SECONDS");
    setenv("SST_ARGUS_DIAGNOSTIC_START", "invalid", 1);
    auto invalid = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(invalid.action, CliAction::kError);
    unsetenv("SST_ARGUS_DIAGNOSTIC_START");
    unsetenv("SST_ENABLE_ARGUS_DIAGNOSTIC");
}

TEST(CliTest, CaptureDiagnosticIsOffUnlessEnvironmentGateEnabled) {
    std::vector<char*> args = {const_cast<char*>("sst_cam_firmware"),
                               const_cast<char*>("--capture-diagnostic")};
    unsetenv("SST_ENABLE_ARGUS_DIAGNOSTIC");
    auto result = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(result.action, CliAction::kError);
    EXPECT_EQ(result.exit_code, 2);
}

TEST(CliTest, CaptureDiagnosticRejectsOtherOptions) {
    std::vector<char*> args = {const_cast<char*>("sst_cam_firmware"),
                               const_cast<char*>("--capture-diagnostic"),
                               const_cast<char*>("--version")};
    auto result = ParseArgs(static_cast<int>(args.size()), args.data(), "dev", "unknown");
    EXPECT_EQ(result.action, CliAction::kError);
    EXPECT_EQ(result.exit_code, 2);
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