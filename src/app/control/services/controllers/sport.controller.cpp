#include "app/control/services/controllers/sport.controller.hpp"

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

auto SplitOnce(std::string_view text, char delim)
    -> std::pair<std::string_view, std::string_view> {
    const auto sp = text.find(delim);
    if (sp == std::string_view::npos) {
        return {text, std::string_view{}};
    }
    return {text.substr(0, sp), text.substr(sp + 1)};
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

auto ParseJson(std::string_view body) -> std::optional<nlohmann::json> {
    try {
        return nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& ex) {
        spdlog::warn("SportController: invalid JSON payload: {}", ex.what());
        return std::nullopt;
    }
}

auto ParseCreate(const nlohmann::json& j) -> std::optional<sst::db::Sport> {
    sst::db::Sport sport;
    auto code = j.find("code");
    auto name = j.find("name");
    auto periods = j.find("periods");
    if (code == j.end() || !code->is_string()) return std::nullopt;
    if (name == j.end() || !name->is_string()) return std::nullopt;
    if (periods == j.end() || !periods->is_number_integer()) return std::nullopt;
    sport.code = code->get<std::string>();
    sport.name = name->get<std::string>();
    sport.periods = periods->get<std::int32_t>();
    if (sport.periods <= 0) {
        return std::nullopt;
    }
    return sport;
}

auto ParseEventKind(const nlohmann::json& j) -> std::optional<sst::db::SportEventKind> {
    sst::db::SportEventKind kind;
    auto sport_id = j.find("sport_id");
    auto code = j.find("code");
    auto name = j.find("name");
    if (sport_id == j.end() || !sport_id->is_number_integer()) return std::nullopt;
    if (code == j.end() || !code->is_string()) return std::nullopt;
    if (name == j.end() || !name->is_string()) return std::nullopt;
    kind.sport_id = sport_id->get<std::int64_t>();
    kind.code = code->get<std::string>();
    kind.name = name->get<std::string>();
    return kind;
}

}  // namespace

SportController::SportController(sst::db::ISportRepository& repo) : repo_(repo) {}

auto SportController::Handle(const Command& cmd) -> CommandResult {
    const auto payload = AsStringView(cmd.payload);
    const auto [verb, rest] = SplitVerb(payload);
    spdlog::info("SportController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "create") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto sport = ParseCreate(*j);
        if (!sport) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto saved = repo_.save(*sport);
        if (!saved.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(std::to_string(saved.data.id));
        return result;
    }

    if (verb == "list") {
        auto rows = repo_.list();
        if (!rows.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        std::string body;
        for (const auto& s : rows.data) {
            body += fmt::format("{} {} {} {}\n", s.id, s.code, s.name, s.periods);
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(body);
        return result;
    }

    if (verb == "delete") {
        auto id = ParseI64(rest);
        if (!id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto removed = repo_.remove(*id);
        if (!removed.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = removed.data ? CommandStatus::kOk : CommandStatus::kNotFound;
        return result;
    }

    if (verb == "add_event_kind") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto kind = ParseEventKind(*j);
        if (!kind) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto saved = repo_.saveEventKind(*kind);
        result.status = saved.success ? CommandStatus::kOk : CommandStatus::kInternal;
        return result;
    }

    if (verb == "list_event_kinds") {
        auto sport_id = ParseI64(rest);
        if (!sport_id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto rows = repo_.listEventKinds(*sport_id);
        if (!rows.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        std::string body;
        for (const auto& k : rows.data) {
            body += fmt::format("{} {}\n", k.code, k.name);
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(body);
        return result;
    }

    if (verb == "remove_event_kind") {
        auto [sid, code] = SplitOnce(rest, ' ');
        auto sport_id = ParseI64(sid);
        if (!sport_id || code.empty()) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto removed = repo_.removeEventKind(*sport_id, std::string{code});
        if (!removed.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = removed.data ? CommandStatus::kOk : CommandStatus::kNotFound;
        return result;
    }

    spdlog::warn("SportController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
