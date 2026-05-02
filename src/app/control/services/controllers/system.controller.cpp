#include "app/control/services/controllers/system.controller.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace sst::control {

namespace {

auto AsStringView(const std::vector<std::byte>& bytes) -> std::string_view {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

auto MakePayload(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> out(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<std::byte>(text[i]);
    }
    return out;
}

auto OptStringField(const std::optional<std::string>& value) -> nlohmann::json {
    return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

auto NowIso8601Utc() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

}  // namespace

SystemController::SystemController(const sst::config::DeviceData& device) : device_(device) {}

auto SystemController::Handle(const Command& cmd) -> CommandResult {
    const auto verb = AsStringView(cmd.payload);
    spdlog::info("SystemController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "version") {
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(device_.version.value_or(""));
        return result;
    }

    if (verb == "info") {
        nlohmann::json j;
        j["name"] = OptStringField(device_.name);
        j["model"] = OptStringField(device_.model);
        j["version"] = OptStringField(device_.version);
        j["manufacturer"] = OptStringField(device_.manufacturer);
        j["serial_number"] = OptStringField(device_.serial_number);
        j["timezone"] = OptStringField(device_.timezone);
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(j.dump());
        return result;
    }

    if (verb == "time") {
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(NowIso8601Utc());
        return result;
    }

    spdlog::warn("SystemController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
