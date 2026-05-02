#include <gtest/gtest.h>

#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/control/services/controllers/system.controller.hpp"
#include "domain/config/models/device.hpp"

namespace {

using sst::config::DeviceData;
using sst::control::Command;
using sst::control::CommandStatus;
using sst::control::SystemController;

auto MakeCommand(std::string_view text) -> Command {
    Command cmd;
    cmd.route = "system";
    cmd.payload.resize(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(text[i]);
    }
    return cmd;
}

auto PayloadToString(const std::vector<std::byte>& bytes) -> std::string {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

auto MakeFullDevice() -> DeviceData {
    DeviceData d;
    d.name = "sst-cam";
    d.model = "v1";
    d.version = "1.0.0";
    d.manufacturer = "Scout Sport Technology";
    d.serial_number = "00000001";
    d.timezone = "UTC";
    return d;
}

TEST(SystemControllerTest, VersionReturnsConfiguredString) {
    auto device = MakeFullDevice();
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("version"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    EXPECT_EQ(PayloadToString(res.payload), "1.0.0");
}

TEST(SystemControllerTest, VersionReturnsEmptyWhenUnset) {
    DeviceData device;  // all fields nullopt
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("version"));
    EXPECT_EQ(res.status, CommandStatus::kOk);
    EXPECT_TRUE(PayloadToString(res.payload).empty());
}

TEST(SystemControllerTest, InfoReturnsFullJson) {
    auto device = MakeFullDevice();
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("info"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    EXPECT_EQ(j["name"].get<std::string>(), "sst-cam");
    EXPECT_EQ(j["model"].get<std::string>(), "v1");
    EXPECT_EQ(j["version"].get<std::string>(), "1.0.0");
    EXPECT_EQ(j["manufacturer"].get<std::string>(), "Scout Sport Technology");
    EXPECT_EQ(j["serial_number"].get<std::string>(), "00000001");
    EXPECT_EQ(j["timezone"].get<std::string>(), "UTC");
}

TEST(SystemControllerTest, InfoFieldsAreNullWhenUnset) {
    DeviceData device;
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("info"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    auto j = nlohmann::json::parse(PayloadToString(res.payload));
    EXPECT_TRUE(j["name"].is_null());
    EXPECT_TRUE(j["model"].is_null());
    EXPECT_TRUE(j["serial_number"].is_null());
}

TEST(SystemControllerTest, TimeIsIso8601Utc) {
    auto device = MakeFullDevice();
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("time"));
    ASSERT_EQ(res.status, CommandStatus::kOk);
    const auto body = PayloadToString(res.payload);
    // Format: YYYY-MM-DDTHH:MM:SS — no timezone suffix, UTC by contract.
    static const std::regex kIso8601{R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$)"};
    EXPECT_TRUE(std::regex_match(body, kIso8601)) << "got: " << body;
}

TEST(SystemControllerTest, UnknownVerbRejected) {
    DeviceData device;
    SystemController controller(device);

    auto res = controller.Handle(MakeCommand("self_destruct"));
    EXPECT_EQ(res.status, CommandStatus::kInvalidArgument);
}

}  // namespace
