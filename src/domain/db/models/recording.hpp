#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::db {

enum class RecordingKind : uint8_t { kFullGame = 0, kEventClip = 1 };

struct Recording {
    std::string id;  // UUID
    std::optional<std::string> match_id;
    RecordingKind kind{RecordingKind::kFullGame};
    std::string file_path;
    std::string started_at;
    std::optional<std::string> ended_at;
};

struct RecordingSegment {
    int64_t id{0};
    std::string recording_id;
    int32_t sequence{0};
    std::string file_path;
    std::string started_at;
    std::optional<std::string> ended_at;
};

struct EventClip {
    std::string id;  // UUID
    std::string match_event_id;
    std::string recording_id;
    std::string file_path;
    int32_t pre_seconds{0};
    int32_t post_seconds{0};
};

}  // namespace sst::db
