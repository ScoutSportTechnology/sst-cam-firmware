#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::db {

struct Team {
    int64_t id{0};
    int64_t sport_id{0};
    std::string name;
    std::string short_name;
};

struct Player {
    int64_t id{0};
    int64_t team_id{0};
    std::string name;
    std::optional<int32_t> jersey_number;
};

}  // namespace sst::db
