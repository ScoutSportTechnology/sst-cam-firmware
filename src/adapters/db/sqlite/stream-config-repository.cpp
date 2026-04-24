#include "adapters/db/sqlite/stream-config-repository.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

SqliteStreamConfigRepository::SqliteStreamConfigRepository(DbConnection& conn) : db_(conn.db()) {}

namespace {

auto readStreamConfig(SQLite::Statement& stmt) -> StreamConfig {
    StreamConfig cfg;
    int col = 0;
    cfg.id = stmt.getColumn(col++).getInt64();
    cfg.user_id = stmt.getColumn(col++).getInt64();
    cfg.platform = static_cast<StreamPlatform>(stmt.getColumn(col++).getInt());
    cfg.name = stmt.getColumn(col++).getText();
    cfg.enabled = static_cast<bool>(stmt.getColumn(col++).getInt());
    cfg.stream_key = stmt.getColumn(col++).getText();
    cfg.stream_type = static_cast<StreamType>(stmt.getColumn(col++).getInt());
    cfg.url = stmt.getColumn(col++).getText();
    cfg.codec = static_cast<StreamCodec>(stmt.getColumn(col++).getInt());
    cfg.width = stmt.getColumn(col++).getInt();
    cfg.height = stmt.getColumn(col++).getInt();
    cfg.framerate = stmt.getColumn(col++).getInt();
    cfg.bitrate_kbps = stmt.getColumn(col).getInt();
    return cfg;
}

void bindStreamConfigFields(SQLite::Statement& stmt, int start_param, const StreamConfig& data) {
    int param = start_param;
    stmt.bind(param++, data.user_id);
    stmt.bind(param++, static_cast<int>(data.platform));
    stmt.bind(param++, data.name);
    stmt.bind(param++, static_cast<int>(data.enabled));
    stmt.bind(param++, data.stream_key);
    stmt.bind(param++, static_cast<int>(data.stream_type));
    stmt.bind(param++, data.url);
    stmt.bind(param++, static_cast<int>(data.codec));
    stmt.bind(param++, data.width);
    stmt.bind(param++, data.height);
    stmt.bind(param++, data.framerate);
    stmt.bind(param, data.bitrate_kbps);
}

}  // namespace

auto SqliteStreamConfigRepository::getAll(int64_t user_id)
    -> DbResult<std::vector<StreamConfig>> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT id, user_id, platform, name, enabled, stream_key, stream_type, "
            "url, codec, width, height, framerate, bitrate_kbps "
            "FROM stream_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        std::vector<StreamConfig> results;
        while (stmt.executeStep()) {
            results.push_back(readStreamConfig(stmt));
        }
        return DbResult<std::vector<StreamConfig>>::ok(std::move(results));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("StreamConfigRepository::getAll failed: {}", ex.what());
        return DbResult<std::vector<StreamConfig>>::fail();
    }
}

auto SqliteStreamConfigRepository::save(const StreamConfig& data) -> DbResult<StreamConfig> {
    try {
        StreamConfig saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(
                db_,
                "INSERT INTO stream_config "
                "(user_id, platform, name, enabled, stream_key, stream_type, "
                "url, codec, width, height, framerate, bitrate_kbps) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            bindStreamConfigFields(stmt, 1, data);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(
                db_,
                "INSERT OR REPLACE INTO stream_config "
                "(id, user_id, platform, name, enabled, stream_key, stream_type, "
                "url, codec, width, height, framerate, bitrate_kbps) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            stmt.bind(1, data.id);
            bindStreamConfigFields(stmt, 2, data);
            stmt.exec();
        }
        return DbResult<StreamConfig>::ok(std::move(saved));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("StreamConfigRepository::save failed: {}", ex.what());
        return DbResult<StreamConfig>::fail();
    }
}

auto SqliteStreamConfigRepository::remove(int64_t stream_id) -> bool {
    try {
        SQLite::Statement stmt(db_, "DELETE FROM stream_config WHERE id = ?");
        stmt.bind(1, stream_id);
        stmt.exec();
        return db_.getChanges() > 0;
    } catch (const SQLite::Exception& ex) {
        spdlog::error("StreamConfigRepository::remove failed: {}", ex.what());
        return false;
    }
}

}  // namespace sst::db
