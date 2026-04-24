#include "adapters/db/sqlite/stream-config-repository.hpp"

namespace sst::db {

SqliteStreamConfigRepository::SqliteStreamConfigRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

namespace {

auto readStreamConfig(SQLite::Statement& stmt) -> StreamConfig {
    ColumnReader col(stmt);
    return {.id = col.nextI64(),
            .user_id = col.nextI64(),
            .platform = col.nextEnum<StreamPlatform>(),
            .name = col.nextText(),
            .enabled = col.nextBool(),
            .stream_key = col.nextText(),
            .stream_type = col.nextEnum<StreamType>(),
            .url = col.nextText(),
            .codec = col.nextEnum<StreamCodec>(),
            .width = col.nextI32(),
            .height = col.nextI32(),
            .framerate = col.nextI32(),
            .bitrate_kbps = col.nextI32()};
}

void bindStreamFields(ParamBinder& binder, const StreamConfig& data) {
    binder.i64(data.user_id)
        .asEnum(data.platform)
        .text(data.name)
        .boolean(data.enabled)
        .text(data.stream_key)
        .asEnum(data.stream_type)
        .text(data.url)
        .asEnum(data.codec)
        .i32(data.width)
        .i32(data.height)
        .i32(data.framerate)
        .i32(data.bitrate_kbps);
}

}  // namespace

auto SqliteStreamConfigRepository::getAll(int64_t user_id)
    -> DbResult<std::vector<StreamConfig>> {
    return dbExecute<std::vector<StreamConfig>>("StreamConfigRepository::getAll", [&] {
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
    });
}

auto SqliteStreamConfigRepository::save(const StreamConfig& data) -> DbResult<StreamConfig> {
    return dbExecute<StreamConfig>("StreamConfigRepository::save", [&] {
        StreamConfig saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(
                db_,
                "INSERT INTO stream_config "
                "(user_id, platform, name, enabled, stream_key, stream_type, "
                "url, codec, width, height, framerate, bitrate_kbps) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            ParamBinder binder(stmt);
            bindStreamFields(binder, data);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(
                db_,
                "INSERT OR REPLACE INTO stream_config "
                "(id, user_id, platform, name, enabled, stream_key, stream_type, "
                "url, codec, width, height, framerate, bitrate_kbps) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            ParamBinder binder(stmt);
            binder.i64(data.id);
            bindStreamFields(binder, data);
            stmt.exec();
        }
        return DbResult<StreamConfig>::ok(std::move(saved));
    });
}

auto SqliteStreamConfigRepository::remove(int64_t stream_id) -> DbResult<bool> {
    return dbExecute<bool>("StreamConfigRepository::remove", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM stream_config WHERE id = ?");
        stmt.bind(1, stream_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

}  // namespace sst::db
