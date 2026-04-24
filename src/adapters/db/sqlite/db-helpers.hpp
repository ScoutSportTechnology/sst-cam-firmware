#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>

#include "adapters/db/sqlite/db-connection.hpp"
#include "domain/db/models/db-result.hpp"

namespace sst::db {

// ── Base class ────────────────────────────────────────────────────────────────

class SqliteRepositoryBase {
   protected:
    SQLite::Database& db_;
    explicit SqliteRepositoryBase(DbConnection& conn) : db_(conn.db()) {}
};

// ── Error-handling wrapper ────────────────────────────────────────────────────

template <typename T>
auto dbExecute(const char* ctx, auto func) -> DbResult<T> {
    try {
        return func();
    } catch (const SQLite::Exception& ex) {
        spdlog::error("{} failed: {}", ctx, ex.what());
        return DbResult<T>::fail();
    }
}

// ── Column reader (auto-incrementing index) ───────────────────────────────────

class ColumnReader {
   public:
    explicit ColumnReader(SQLite::Statement& stmt) : stmt_(stmt) {}

    auto nextI64() -> int64_t { return stmt_.getColumn(col_++).getInt64(); }
    auto nextI32() -> int { return stmt_.getColumn(col_++).getInt(); }
    auto nextBool() -> bool { return static_cast<bool>(stmt_.getColumn(col_++).getInt()); }
    auto nextF32() -> float { return static_cast<float>(stmt_.getColumn(col_++).getDouble()); }
    auto nextText() -> std::string { return stmt_.getColumn(col_++).getText(); }

    auto nextOptText() -> std::optional<std::string> {
        const auto& col = stmt_.getColumn(col_++);
        if (col.isNull()) {
            return std::nullopt;
        }
        return std::string{col.getText()};
    }

    template <typename E>
    auto nextEnum() -> E {
        return static_cast<E>(nextI32());
    }

   private:
    SQLite::Statement& stmt_;
    int col_{0};
};

// ── Parameter binder (fluent, auto-incrementing index) ───────────────────────

class ParamBinder {
   public:
    explicit ParamBinder(SQLite::Statement& stmt) : stmt_(stmt) {}

    auto i64(int64_t val) -> ParamBinder& {
        stmt_.bind(idx_++, val);
        return *this;
    }
    auto i32(int val) -> ParamBinder& {
        stmt_.bind(idx_++, val);
        return *this;
    }
    auto boolean(bool val) -> ParamBinder& {
        stmt_.bind(idx_++, static_cast<int>(val));
        return *this;
    }
    auto real(float val) -> ParamBinder& {
        stmt_.bind(idx_++, static_cast<double>(val));
        return *this;
    }
    auto text(const std::string& val) -> ParamBinder& {
        stmt_.bind(idx_++, val);
        return *this;
    }
    auto optText(const std::optional<std::string>& val) -> ParamBinder& {
        if (val) {
            stmt_.bind(idx_++, *val);
        } else {
            stmt_.bind(idx_++);
        }
        return *this;
    }
    template <typename E>
    auto asEnum(E val) -> ParamBinder& {
        return i32(static_cast<int>(val));
    }

   private:
    SQLite::Statement& stmt_;
    int idx_{1};
};

}  // namespace sst::db
