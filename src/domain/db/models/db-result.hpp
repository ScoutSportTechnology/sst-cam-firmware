#pragma once

#include <utility>

namespace sst::db {

template <typename T>
struct DbResult {
    bool success{false};
    T data{};

    static auto ok(T value) -> DbResult {
        DbResult res;
        res.success = true;
        res.data = std::move(value);
        return res;
    }

    static auto fail() -> DbResult { return {}; }
};

}  // namespace sst::db
