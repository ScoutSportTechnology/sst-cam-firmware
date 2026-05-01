#pragma once

#include <cstdint>
#include <string>

namespace sst::db {

struct Sport {
    int64_t id{0};
    std::string code;
    std::string name;
    int32_t periods{0};
};

struct SportEventKind {
    int64_t sport_id{0};
    std::string code;
    std::string name;
};

}  // namespace sst::db
