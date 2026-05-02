#include "app/control/services/controllers/match.controller.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "domain/db/models/match.hpp"
#include "domain/db/models/recording.hpp"

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

auto SerializeMatch(const sst::db::Match& m) -> nlohmann::json {
    nlohmann::json j;
    j["id"] = m.id;
    j["user_id"] = m.user_id;
    j["sport_id"] = m.sport_id;
    j["team_a_id"] = m.team_a_id;
    j["team_b_id"] = m.team_b_id;
    j["name"] = m.name.has_value() ? nlohmann::json(*m.name) : nlohmann::json(nullptr);
    j["started_at"] = m.started_at;
    j["ended_at"] =
        m.ended_at.has_value() ? nlohmann::json(*m.ended_at) : nlohmann::json(nullptr);
    j["current_period"] = m.current_period;
    j["final_score_a"] = m.final_score_a;
    j["final_score_b"] = m.final_score_b;
    return j;
}

auto SerializeMatchEvent(const sst::db::MatchEvent& e) -> nlohmann::json {
    nlohmann::json j;
    j["id"] = e.id;
    j["match_id"] = e.match_id;
    j["sport_id"] = e.sport_id;
    j["event_code"] = e.event_code;
    j["period"] = e.period;
    j["timestamp_in_match"] = e.timestamp_in_match;
    j["wall_clock_at"] = e.wall_clock_at;
    if (e.metadata_json.has_value()) {
        try {
            // Re-parse so the output is structured JSON rather than a string.
            j["metadata"] = nlohmann::json::parse(*e.metadata_json);
        } catch (const nlohmann::json::exception&) {
            j["metadata"] = *e.metadata_json;
        }
    } else {
        j["metadata"] = nullptr;
    }
    return j;
}

auto SerializeEventClip(const sst::db::EventClip& c) -> nlohmann::json {
    nlohmann::json j;
    j["id"] = c.id;
    j["match_event_id"] = c.match_event_id;
    j["recording_id"] = c.recording_id;
    j["file_path"] = c.file_path;
    j["pre_seconds"] = c.pre_seconds;
    j["post_seconds"] = c.post_seconds;
    return j;
}

auto SerializeRecording(const sst::db::Recording& r) -> nlohmann::json {
    nlohmann::json j;
    j["id"] = r.id;
    j["match_id"] = r.match_id.has_value() ? nlohmann::json(*r.match_id) : nlohmann::json(nullptr);
    j["kind"] = static_cast<int>(r.kind);
    j["file_path"] = r.file_path;
    j["started_at"] = r.started_at;
    j["ended_at"] =
        r.ended_at.has_value() ? nlohmann::json(*r.ended_at) : nlohmann::json(nullptr);
    return j;
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

MatchController::MatchController(sst::match::IMatchService& service,
                                 sst::db::IMatchRepository& match_repo,
                                 sst::db::IRecordingRepository& recording_repo,
                                 std::int64_t user_id)
    : service_(service),
      match_repo_(match_repo),
      recording_repo_(recording_repo),
      user_id_(user_id) {}

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

    if (verb == "list") {
        auto rows = match_repo_.listByUser(user_id_);
        if (!rows.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        nlohmann::json out = nlohmann::json::array();
        for (const auto& m : rows.data) {
            out.push_back(SerializeMatch(m));
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(out.dump());
        return result;
    }

    if (verb == "get") {
        if (rest.empty()) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        const std::string match_id{rest};
        auto match = match_repo_.get(match_id);
        if (!match.success) {
            result.status = CommandStatus::kNotFound;
            return result;
        }
        auto events = match_repo_.listEvents(match_id);
        if (!events.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        nlohmann::json out;
        out["match"] = SerializeMatch(match.data);
        out["events"] = nlohmann::json::array();
        for (const auto& e : events.data) {
            out["events"].push_back(SerializeMatchEvent(e));
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(out.dump());
        return result;
    }

    if (verb == "list_clips") {
        if (rest.empty()) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        const std::string match_id{rest};
        auto clips = recording_repo_.listEventClipsForMatch(match_id);
        if (!clips.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        nlohmann::json out = nlohmann::json::array();
        for (const auto& c : clips.data) {
            out.push_back(SerializeEventClip(c));
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(out.dump());
        return result;
    }

    if (verb == "get_clip") {
        if (rest.empty()) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        const std::string clip_id{rest};
        auto clip = recording_repo_.getEventClip(clip_id);
        if (!clip.success) {
            result.status = CommandStatus::kNotFound;
            return result;
        }
        auto recording = recording_repo_.get(clip.data.recording_id);
        nlohmann::json out;
        out["clip"] = SerializeEventClip(clip.data);
        out["recording"] = recording.success ? SerializeRecording(recording.data)
                                             : nlohmann::json(nullptr);
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(out.dump());
        return result;
    }

    spdlog::warn("MatchController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
