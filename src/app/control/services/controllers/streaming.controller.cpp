#include "app/control/services/controllers/streaming.controller.hpp"

#include <string_view>

#include <spdlog/spdlog.h>

#include "domain/streaming/models/preview-config.hpp"

namespace sst::control {

namespace {

auto AsStringView(const std::vector<std::byte>& bytes) -> std::string_view {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

StreamingController::StreamingController(sst::streaming::IPreviewServer& preview)
    : preview_(preview) {}

auto StreamingController::Handle(const Command& cmd) -> CommandResult {
    const auto verb = AsStringView(cmd.payload);
    spdlog::info("StreamingController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "start_preview") {
        sst::streaming::PreviewConfig cfg;  // defaults: 854x480 @ 30fps, 1.5 Mbps, /preview, 8554
        result.status =
            preview_.Start(cfg) ? CommandStatus::kOk : CommandStatus::kInternal;
        return result;
    }

    if (verb == "stop_preview") {
        preview_.Stop();
        result.status = CommandStatus::kOk;
        return result;
    }

    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
