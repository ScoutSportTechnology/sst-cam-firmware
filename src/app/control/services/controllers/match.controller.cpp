#include "app/control/services/controllers/match.controller.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
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

auto SplitVerb(std::string_view payload) -> std::pair<std::string_view, std::string_view> {
    const auto sp = payload.find(' ');
    if (sp == std::string_view::npos) {
        return {payload, std::string_view{}};
    }
    return {payload.substr(0, sp), payload.substr(sp + 1)};
}

auto ParseI32(std::string_view s) -> std::optional<std::int32_t> {
    std::int32_t out = 0;
    const auto* begin = s.data();
    const auto* end = s.data() + s.size();
    auto res = std::from_chars(begin, end, out);
    if (res.ec != std::errc{} || res.ptr != end) {
        return std::nullopt;
    }
    return out;
}

auto ParseStartRequest(const nlohmann::json& j) -> std::optional<sst::match::StartMatchRequest> {
    sst::match::StartMatchRequest req;
    auto sport_code = j.find("sport_code");
    auto team_a = j.find("team_a_id");
    auto team_b = j.find("team_b_id");
    if (sport_code == j.end() || !sport_code->is_string()) return std::nullopt;
    if (team_a == j.end() || !team_a->is_number_integer()) return std::nullopt;
    if (team_b == j.end() || !team_b->is_number_integer()) return std::nullopt;
    req.sport_code = sport_code->get<std::string>();
    req.team_a_id = team_a->get<std::int64_t>();
    req.team_b_id = team_b->get<std::int64_t>();
    auto name = j.find("name");
    if (name != j.end() && name->is_string()) {
        req.name = name->get<std::string>();
    }
    return req;
}

auto ParseScoreRequest(const nlohmann::json& j) -> std::optional<sst::match::SetScoreRequest> {
    sst::match::SetScoreRequest req;
    auto team = j.find("team");
    auto score = j.find("score");
    if (team == j.end() || !team->is_string()) return std::nullopt;
    if (score == j.end() || !score->is_number_integer()) return std::nullopt;
    const auto t = team->get<std::string>();
    if (t == "a" || t == "A") {
        req.team = sst::match::TeamSide::kA;
    } else if (t == "b" || t == "B") {
        req.team = sst::match::TeamSide::kB;
    } else {
        return std::nullopt;
    }
    req.score = score->get<std::int32_t>();
    return req;
}

auto ParseEventRequest(const nlohmann::json& j) -> std::optional<sst::match::RecordEventRequest> {
    sst::match::RecordEventRequest req;
    auto code = j.find("event_code");
    if (code == j.end() || !code->is_string()) return std::nullopt;
    req.event_code = code->get<std::string>();
    auto meta = j.find("metadata");
    if (meta != j.end()) {
        // Re-serialize metadata as a JSON string so DB stores it verbatim.
        req.metadata_json = meta->dump();
    }
    return req;
}

auto ParseJson(std::string_view body) -> std::optional<nlohmann::json> {
    try {
        return nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& ex) {
        spdlog::warn("MatchController: invalid JSON payload: {}", ex.what());
        return std::nullopt;
    }
}

}  // namespace

MatchController::MatchController(sst::match::IMatchService& service) : service_(service) {}

auto MatchController::Handle(const Command& cmd) -> CommandResult {
    const auto payload = AsStringView(cmd.payload);
    const auto [verb, rest] = SplitVerb(payload);
    spdlog::info("MatchController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "start") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto req = ParseStartRequest(*j);
        if (!req) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto res = service_.StartMatch(*req);
        if (!res.success) {
            result.status = CommandStatus::kFailedPrecondition;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(fmt::format("{}\n{}", res.match_id, res.recording_id));
        return result;
    }

    if (verb == "set_period") {
        auto period = ParseI32(rest);
        if (!period) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status = service_.SetPeriod(*period) ? CommandStatus::kOk
                                                    : CommandStatus::kFailedPrecondition;
        return result;
    }

    if (verb == "set_score") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto req = ParseScoreRequest(*j);
        if (!req) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status = service_.SetScore(*req) ? CommandStatus::kOk
                                                : CommandStatus::kFailedPrecondition;
        return result;
    }

    if (verb == "record_event") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto req = ParseEventRequest(*j);
        if (!req) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto res = service_.RecordEvent(*req);
        if (!res.success) {
            result.status = CommandStatus::kFailedPrecondition;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(
            fmt::format("{}\n{}\n{}", res.match_event_id, res.event_clip_id,
                        res.file_path.string()));
        return result;
    }

    if (verb == "end") {
        auto res = service_.EndMatch();
        if (!res.success) {
            result.status = CommandStatus::kFailedPrecondition;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(res.merged_path.string());
        return result;
    }

    spdlog::warn("MatchController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
