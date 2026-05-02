#include "app/control/services/controllers/camera.controller.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "domain/db/models/camera.hpp"

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

auto ParseJson(std::string_view body) -> std::optional<nlohmann::json> {
    try {
        return nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& ex) {
        spdlog::warn("CameraController: invalid JSON payload: {}", ex.what());
        return std::nullopt;
    }
}

auto SerializeConfig(const sst::db::CameraConfig& cfg) -> std::string {
    nlohmann::json j;
    j["user_id"] = cfg.user_id;
    j["exposure"] = cfg.exposure;
    j["gain"] = cfg.gain;
    j["white_balance"] = static_cast<std::uint8_t>(cfg.white_balance);
    j["focus"] = static_cast<std::uint8_t>(cfg.focus);
    j["width"] = cfg.width;
    j["height"] = cfg.height;
    j["format"] = static_cast<std::uint8_t>(cfg.format);
    j["fps"] = cfg.fps;
    j["event_clip_pre_seconds"] = cfg.event_clip_pre_seconds;
    j["event_clip_post_seconds"] = cfg.event_clip_post_seconds;
    return j.dump();
}

// Overlay JSON fields onto `cfg`. Returns false if any provided field has the
// wrong type or fails the same CHECK constraints the schema enforces — better
// to reject up-front than let SqliteCpp throw on save.
auto ApplyPatch(const nlohmann::json& patch, sst::db::CameraConfig& cfg) -> bool {
    auto get_positive_i32 = [&](const char* key, std::int32_t& dst) -> bool {
        auto it = patch.find(key);
        if (it == patch.end()) return true;
        if (!it->is_number_integer()) return false;
        const auto v = it->get<std::int32_t>();
        if (v <= 0) return false;
        dst = v;
        return true;
    };
    auto get_enum_u8 = [&]<typename E>(const char* key, E& dst) -> bool {
        auto it = patch.find(key);
        if (it == patch.end()) return true;
        if (!it->is_number_integer()) return false;
        dst = static_cast<E>(it->get<std::uint8_t>());
        return true;
    };

    if (!get_positive_i32("exposure", cfg.exposure)) return false;
    if (auto it = patch.find("gain"); it != patch.end()) {
        if (!it->is_number()) return false;
        const auto v = it->get<float>();
        if (v <= 0.0F) return false;
        cfg.gain = v;
    }
    if (!get_enum_u8.template operator()<sst::db::CameraWhiteBalance>("white_balance",
                                                                      cfg.white_balance)) {
        return false;
    }
    if (!get_enum_u8.template operator()<sst::db::CameraFocus>("focus", cfg.focus)) {
        return false;
    }
    if (!get_positive_i32("width", cfg.width)) return false;
    if (!get_positive_i32("height", cfg.height)) return false;
    if (!get_enum_u8.template operator()<sst::common::PixelFormat>("format", cfg.format)) {
        return false;
    }
    if (!get_positive_i32("fps", cfg.fps)) return false;
    if (!get_positive_i32("event_clip_pre_seconds", cfg.event_clip_pre_seconds)) return false;
    if (!get_positive_i32("event_clip_post_seconds", cfg.event_clip_post_seconds)) return false;
    return true;
}

}  // namespace

CameraController::CameraController(sst::db::ICameraRepository& repo, std::int64_t user_id)
    : repo_(repo), user_id_(user_id) {}

auto CameraController::Handle(const Command& cmd) -> CommandResult {
    const auto payload = AsStringView(cmd.payload);
    const auto [verb, rest] = SplitVerb(payload);
    spdlog::info("CameraController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "get_config") {
        auto current = repo_.getConfig(user_id_);
        if (!current.success) {
            result.status = CommandStatus::kNotFound;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(SerializeConfig(current.data));
        return result;
    }

    if (verb == "set_config") {
        auto j = ParseJson(rest);
        if (!j || !j->is_object()) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto current = repo_.getConfig(user_id_);
        if (!current.success) {
            result.status = CommandStatus::kNotFound;
            return result;
        }
        if (!ApplyPatch(*j, current.data)) {
            result.status = CommandStatus::kInvalidArgument;
            return result;
        }
        auto saved = repo_.saveConfig(current.data);
        if (!saved.success) {
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(SerializeConfig(saved.data));
        return result;
    }

    spdlog::warn("CameraController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
