#pragma once

#include <cstdint>
#include <string>

namespace sst::db {

struct User {
    int64_t id{0};
    std::string username;
};

}  // namespace sst::db
