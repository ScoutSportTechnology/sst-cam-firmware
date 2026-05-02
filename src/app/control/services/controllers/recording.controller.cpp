#include "app/control/services/controllers/recording.controller.hpp"

#include <string_view>

#include <spdlog/spdlog.h>

namespace sst::control {

namespace {

auto AsStringView(const std::vector<std::byte>& bytes) -> std::string_view {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

RecordingController::RecordingController(sst::storage::IRecordingService& service)
    : service_(service) {}

auto RecordingController::Handle(const Command& cmd) -> CommandResult {
    const auto verb = AsStringView(cmd.payload);
    spdlog::info("RecordingController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "pause") {
        result.status =
            service_.Pause() ? CommandStatus::kOk : CommandStatus::kFailedPrecondition;
        return result;
    }

    if (verb == "resume") {
        result.status =
            service_.Resume() ? CommandStatus::kOk : CommandStatus::kFailedPrecondition;
        return result;
    }

    spdlog::warn("RecordingController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
