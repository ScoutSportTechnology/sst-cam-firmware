#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <string>

namespace sst::db {

class DbConnection {
   public:
    explicit DbConnection(const std::string& path);
    auto db() -> SQLite::Database&;

   private:
    SQLite::Database db_;
};

}  // namespace sst::db
