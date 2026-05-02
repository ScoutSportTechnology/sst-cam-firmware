#include "app/control/services/controllers/streaming.controller.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "domain/streaming/models/app-stream-config.hpp"
#include "domain/streaming/models/platform-stream-config.hpp"

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

// Splits the payload into (verb, rest). Verb is everything up to the first
// space; rest is the suffix (or empty).
auto SplitVerb(std::string_view payload) -> std::pair<std::string_view, std::string_view> {
    const auto sp = payload.find(' ');
    if (sp == std::string_view::npos) {
        return {payload, std::string_view{}};
    }
    return {payload.substr(0, sp), payload.substr(sp + 1)};
}

auto ParseI64(std::string_view s) -> std::optional<std::int64_t> {
    std::int64_t out = 0;
    const auto* begin = s.data();
    const auto* end = s.data() + s.size();
    auto res = std::from_chars(begin, end, out);
    if (res.ec != std::errc{} || res.ptr != end) {
        return std::nullopt;
    }
    return out;
}

auto ToPlatformConfig(const sst::db::StreamConfig& row) -> sst::streaming::PlatformStreamConfig {
    sst::streaming::PlatformStreamConfig cfg;
    cfg.stream_id = row.id;
    cfg.name = row.name;
    cfg.type = row.stream_type == sst::db::StreamType::kRtmps
                   ? sst::streaming::PlatformStreamType::kRtmpS
                   : sst::streaming::PlatformStreamType::kRtmp;
    cfg.url = row.url.value_or("");
    cfg.stream_key = row.stream_key.value_or("");
    cfg.codec = row.codec == sst::db::StreamCodec::kH265
                    ? sst::streaming::PlatformStreamCodec::kH265
                    : sst::streaming::PlatformStreamCodec::kH264;
    cfg.width = row.width;
    cfg.height = row.height;
    cfg.framerate = row.framerate;
    cfg.bitrate_kbps = row.bitrate_kbps;
    return cfg;
}

}  // namespace

StreamingController::StreamingController(sst::streaming::IStreamingService& service,
                                         sst::db::IStreamConfigRepository& stream_config_repo)
    : service_(service), stream_config_repo_(stream_config_repo) {}

auto StreamingController::Handle(const Command& cmd) -> CommandResult {
    const auto payload = AsStringView(cmd.payload);
    const auto [verb, rest] = SplitVerb(payload);
    spdlog::info("StreamingController: verb=\"{}\" rest=\"{}\"", verb, rest);

    CommandResult result;

    if (verb == "start_app_stream") {
        sst::streaming::AppStreamConfig cfg;  // defaults: 854x480 @ 30fps, 1.5 Mbps, /preview
        result.status =
            service_.StartAppStream(cfg) ? CommandStatus::kOk : CommandStatus::kInternal;
        return result;
    }

    if (verb == "stop_app_stream") {
        result.status =
            service_.StopAppStream() ? CommandStatus::kOk : CommandStatus::kFailedPrecondition;
        return result;
    }

    if (verb == "start_platform_stream") {
        const auto id = ParseI64(rest);
        if (!id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto row = stream_config_repo_.get(*id);
        if (!row.success) {
            result.status = CommandStatus::kNotFound;
            return result;
        }
        if (row.data.platform == sst::db::StreamPlatform::kCompanionApp) {
            // The companion-app row is for the always-on RTSP stream, not for
            // RTMP push — guard against starting it via this verb.
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status = service_.StartPlatformStream(ToPlatformConfig(row.data))
                            ? CommandStatus::kOk
                            : CommandStatus::kInternal;
        return result;
    }

    if (verb == "stop_platform_stream") {
        const auto id = ParseI64(rest);
        if (!id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status =
            service_.StopPlatformStream(*id) ? CommandStatus::kOk : CommandStatus::kNotFound;
        return result;
    }

    if (verb == "list_streams") {
        std::string body =
            fmt::format("app_stream_running={}\n", service_.IsAppStreamRunning() ? 1 : 0);
        for (const auto& s : service_.ListActivePlatformStreams()) {
            body += fmt::format("{} {}\n", s.stream_id, s.name);
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(body);
        return result;
    }

    spdlog::warn("StreamingController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
