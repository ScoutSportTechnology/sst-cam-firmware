#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::db {

struct Match {
    static constexpr int32_t kDefaultCurrentPeriod = 1;

    std::string id;  // UUID
    int64_t user_id{0};
    int64_t sport_id{0};
    int64_t team_a_id{0};
    int64_t team_b_id{0};
    std::optional<std::string> name;
    std::string started_at;
    std::optional<std::string> ended_at;
    int32_t current_period{kDefaultCurrentPeriod};
    int32_t final_score_a{0};
    int32_t final_score_b{0};
};

struct MatchEvent {
    std::string id;  // UUID
    std::string match_id;
    int64_t sport_id{0};
    std::string event_code;
    int32_t period{1};
    double timestamp_in_match{0.0};
    std::string wall_clock_at;
    std::optional<std::string> metadata_json;
};

}  // namespace sst::db
