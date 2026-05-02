#include "app/control/services/controllers/team.controller.hpp"

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
        spdlog::warn("TeamController: invalid JSON payload: {}", ex.what());
        return std::nullopt;
    }
}

auto ParseTeam(const nlohmann::json& j) -> std::optional<sst::db::Team> {
    sst::db::Team team;
    auto sport_id = j.find("sport_id");
    auto name = j.find("name");
    auto short_name = j.find("short_name");
    if (sport_id == j.end() || !sport_id->is_number_integer()) return std::nullopt;
    if (name == j.end() || !name->is_string()) return std::nullopt;
    if (short_name == j.end() || !short_name->is_string()) return std::nullopt;
    team.sport_id = sport_id->get<std::int64_t>();
    team.name = name->get<std::string>();
    team.short_name = short_name->get<std::string>();
    return team;
}

auto ParsePlayer(const nlohmann::json& j) -> std::optional<sst::db::Player> {
    sst::db::Player player;
    auto team_id = j.find("team_id");
    auto name = j.find("name");
    if (team_id == j.end() || !team_id->is_number_integer()) return std::nullopt;
    if (name == j.end() || !name->is_string()) return std::nullopt;
    player.team_id = team_id->get<std::int64_t>();
    player.name = name->get<std::string>();
    auto jersey = j.find("jersey_number");
    if (jersey != j.end() && jersey->is_number_integer()) {
        player.jersey_number = jersey->get<std::int32_t>();
    }
    return player;
}

}  // namespace

TeamController::TeamController(sst::db::ITeamRepository& repo) : repo_(repo) {}

auto TeamController::Handle(const Command& cmd) -> CommandResult {
    const auto payload = AsStringView(cmd.payload);
    const auto [verb, rest] = SplitVerb(payload);
    spdlog::info("TeamController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "create") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto team = ParseTeam(*j);
        if (!team) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto saved = repo_.save(*team);
        if (!saved.success) {
            // FK violation (unknown sport_id) or unique violation (duplicate name).
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(std::to_string(saved.data.id));
        return result;
    }

    if (verb == "list") {
        auto sport_id = ParseI64(rest);
        if (!sport_id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto rows = repo_.list(*sport_id);
        if (!rows.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        std::string body;
        for (const auto& t : rows.data) {
            body += fmt::format("{} {} {}\n", t.id, t.name, t.short_name);
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

    if (verb == "add_player") {
        auto j = ParseJson(rest);
        if (!j) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto player = ParsePlayer(*j);
        if (!player) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto saved = repo_.savePlayer(*player);
        if (!saved.success) {
            // FK violation (unknown team_id) or unique violation (duplicate jersey).
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(std::to_string(saved.data.id));
        return result;
    }

    if (verb == "list_players") {
        auto team_id = ParseI64(rest);
        if (!team_id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto rows = repo_.listPlayers(*team_id);
        if (!rows.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        std::string body;
        for (const auto& p : rows.data) {
            const std::string jersey =
                p.jersey_number.has_value() ? std::to_string(*p.jersey_number) : "-";
            body += fmt::format("{} {} {}\n", p.id, p.name, jersey);
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(body);
        return result;
    }

    if (verb == "remove_player") {
        auto id = ParseI64(rest);
        if (!id) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto removed = repo_.removePlayer(*id);
        if (!removed.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = removed.data ? CommandStatus::kOk : CommandStatus::kNotFound;
        return result;
    }

    spdlog::warn("TeamController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
