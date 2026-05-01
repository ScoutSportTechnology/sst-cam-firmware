#include "adapters/db/sqlite/recording-repository.hpp"

namespace sst::db {

SqliteRecordingRepository::SqliteRecordingRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

namespace {

auto readRecording(SQLite::Statement& stmt) -> Recording {
    ColumnReader col(stmt);
    return {.id = col.nextText(),
            .match_id = col.nextOptText(),
            .kind = col.nextEnum<RecordingKind>(),
            .file_path = col.nextText(),
            .started_at = col.nextText(),
            .ended_at = col.nextOptText()};
}

auto readSegment(SQLite::Statement& stmt) -> RecordingSegment {
    ColumnReader col(stmt);
    return {.id = col.nextI64(),
            .recording_id = col.nextText(),
            .sequence = col.nextI32(),
            .file_path = col.nextText(),
            .started_at = col.nextText(),
            .ended_at = col.nextOptText()};
}

auto readEventClip(SQLite::Statement& stmt) -> EventClip {
    ColumnReader col(stmt);
    return {.id = col.nextText(),
            .match_event_id = col.nextText(),
            .recording_id = col.nextText(),
            .file_path = col.nextText(),
            .pre_seconds = col.nextI32(),
            .post_seconds = col.nextI32()};
}

constexpr const char* kSelectRecordingCols =
    "SELECT id, match_id, kind, file_path, started_at, ended_at FROM recording";

constexpr const char* kSelectSegmentCols =
    "SELECT id, recording_id, sequence, file_path, started_at, ended_at FROM recording_segment";

constexpr const char* kSelectEventClipCols =
    "SELECT id, match_event_id, recording_id, file_path, pre_seconds, post_seconds "
    "FROM event_clip";

}  // namespace

auto SqliteRecordingRepository::save(const Recording& data) -> DbResult<Recording> {
    return dbExecute<Recording>("RecordingRepository::save", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO recording (id, match_id, kind, file_path, started_at, "
            "ended_at) VALUES (?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .text(data.id)
            .optText(data.match_id)
            .asEnum(data.kind)
            .text(data.file_path)
            .text(data.started_at)
            .optText(data.ended_at);
        stmt.exec();
        return DbResult<Recording>::ok(data);
    });
}

auto SqliteRecordingRepository::get(const std::string& recording_id) -> DbResult<Recording> {
    return dbExecute<Recording>("RecordingRepository::get", [&] {
        SQLite::Statement stmt(db_, std::string{kSelectRecordingCols} + " WHERE id = ?");
        stmt.bind(1, recording_id);
        if (!stmt.executeStep()) {
            return DbResult<Recording>::fail();
        }
        return DbResult<Recording>::ok(readRecording(stmt));
    });
}

auto SqliteRecordingRepository::listByMatch(const std::string& match_id)
    -> DbResult<std::vector<Recording>> {
    return dbExecute<std::vector<Recording>>("RecordingRepository::listByMatch", [&] {
        SQLite::Statement stmt(
            db_, std::string{kSelectRecordingCols} + " WHERE match_id = ? ORDER BY started_at");
        stmt.bind(1, match_id);
        std::vector<Recording> rows;
        while (stmt.executeStep()) {
            rows.push_back(readRecording(stmt));
        }
        return DbResult<std::vector<Recording>>::ok(std::move(rows));
    });
}

auto SqliteRecordingRepository::finalize(const std::string& recording_id,
                                         const std::string& ended_at) -> DbResult<bool> {
    return dbExecute<bool>("RecordingRepository::finalize", [&] {
        SQLite::Statement stmt(db_, "UPDATE recording SET ended_at = ? WHERE id = ?");
        ParamBinder(stmt).text(ended_at).text(recording_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteRecordingRepository::remove(const std::string& recording_id) -> DbResult<bool> {
    return dbExecute<bool>("RecordingRepository::remove", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM recording WHERE id = ?");
        stmt.bind(1, recording_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteRecordingRepository::saveSegment(const RecordingSegment& data)
    -> DbResult<RecordingSegment> {
    return dbExecute<RecordingSegment>("RecordingRepository::saveSegment", [&] {
        RecordingSegment saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(
                db_,
                "INSERT INTO recording_segment "
                "(recording_id, sequence, file_path, started_at, ended_at) "
                "VALUES (?, ?, ?, ?, ?)");
            ParamBinder(stmt)
                .text(data.recording_id)
                .i32(data.sequence)
                .text(data.file_path)
                .text(data.started_at)
                .optText(data.ended_at);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(
                db_,
                "INSERT OR REPLACE INTO recording_segment "
                "(id, recording_id, sequence, file_path, started_at, ended_at) "
                "VALUES (?, ?, ?, ?, ?, ?)");
            ParamBinder(stmt)
                .i64(data.id)
                .text(data.recording_id)
                .i32(data.sequence)
                .text(data.file_path)
                .text(data.started_at)
                .optText(data.ended_at);
            stmt.exec();
        }
        return DbResult<RecordingSegment>::ok(std::move(saved));
    });
}

auto SqliteRecordingRepository::listSegments(const std::string& recording_id)
    -> DbResult<std::vector<RecordingSegment>> {
    return dbExecute<std::vector<RecordingSegment>>("RecordingRepository::listSegments", [&] {
        SQLite::Statement stmt(
            db_, std::string{kSelectSegmentCols} + " WHERE recording_id = ? ORDER BY sequence");
        stmt.bind(1, recording_id);
        std::vector<RecordingSegment> rows;
        while (stmt.executeStep()) {
            rows.push_back(readSegment(stmt));
        }
        return DbResult<std::vector<RecordingSegment>>::ok(std::move(rows));
    });
}

auto SqliteRecordingRepository::finalizeSegment(int64_t segment_id, const std::string& ended_at)
    -> DbResult<bool> {
    return dbExecute<bool>("RecordingRepository::finalizeSegment", [&] {
        SQLite::Statement stmt(db_, "UPDATE recording_segment SET ended_at = ? WHERE id = ?");
        ParamBinder(stmt).text(ended_at).i64(segment_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteRecordingRepository::saveEventClip(const EventClip& data) -> DbResult<EventClip> {
    return dbExecute<EventClip>("RecordingRepository::saveEventClip", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO event_clip "
            "(id, match_event_id, recording_id, file_path, pre_seconds, post_seconds) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .text(data.id)
            .text(data.match_event_id)
            .text(data.recording_id)
            .text(data.file_path)
            .i32(data.pre_seconds)
            .i32(data.post_seconds);
        stmt.exec();
        return DbResult<EventClip>::ok(data);
    });
}

auto SqliteRecordingRepository::getEventClip(const std::string& event_clip_id)
    -> DbResult<EventClip> {
    return dbExecute<EventClip>("RecordingRepository::getEventClip", [&] {
        SQLite::Statement stmt(db_, std::string{kSelectEventClipCols} + " WHERE id = ?");
        stmt.bind(1, event_clip_id);
        if (!stmt.executeStep()) {
            return DbResult<EventClip>::fail();
        }
        return DbResult<EventClip>::ok(readEventClip(stmt));
    });
}

auto SqliteRecordingRepository::listEventClipsForMatch(const std::string& match_id)
    -> DbResult<std::vector<EventClip>> {
    return dbExecute<std::vector<EventClip>>(
        "RecordingRepository::listEventClipsForMatch", [&] {
            SQLite::Statement stmt(
                db_,
                "SELECT ec.id, ec.match_event_id, ec.recording_id, ec.file_path, "
                "       ec.pre_seconds, ec.post_seconds "
                "FROM event_clip ec "
                "JOIN match_event me ON me.id = ec.match_event_id "
                "WHERE me.match_id = ? ORDER BY me.timestamp_in_match");
            stmt.bind(1, match_id);
            std::vector<EventClip> rows;
            while (stmt.executeStep()) {
                rows.push_back(readEventClip(stmt));
            }
            return DbResult<std::vector<EventClip>>::ok(std::move(rows));
        });
}

}  // namespace sst::db
