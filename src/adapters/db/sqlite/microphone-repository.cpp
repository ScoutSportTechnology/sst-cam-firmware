#include "adapters/db/sqlite/microphone-repository.hpp"

namespace sst::db {

SqliteMicrophoneRepository::SqliteMicrophoneRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

auto SqliteMicrophoneRepository::getConfig(int64_t user_id) -> DbResult<MicrophoneConfig> {
    return dbExecute<MicrophoneConfig>("MicrophoneRepository::getConfig", [&] {
        SQLite::Statement stmt(
            db_, "SELECT user_id, noise_reduction FROM microphone_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<MicrophoneConfig>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<MicrophoneConfig>::ok(
            {.user_id = col.nextI64(), .noise_reduction = col.nextBool()});
    });
}

auto SqliteMicrophoneRepository::saveConfig(const MicrophoneConfig& data)
    -> DbResult<MicrophoneConfig> {
    return dbExecute<MicrophoneConfig>("MicrophoneRepository::saveConfig", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO microphone_config (user_id, noise_reduction) VALUES (?, ?)");
        ParamBinder(stmt).i64(data.user_id).boolean(data.noise_reduction);
        stmt.exec();
        return DbResult<MicrophoneConfig>::ok(data);
    });
}

auto SqliteMicrophoneRepository::getLatestCalibration(int32_t mic_id)
    -> DbResult<MicrophoneCalibration> {
    return dbExecute<MicrophoneCalibration>("MicrophoneRepository::getLatestCalibration", [&] {
        SQLite::Statement stmt(
            db_,
            "SELECT id, mic_id, sensitivity, calibrated_at "
            "FROM microphone_calibration WHERE mic_id = ? "
            "ORDER BY calibrated_at DESC LIMIT 1");
        stmt.bind(1, mic_id);
        if (!stmt.executeStep()) {
            return DbResult<MicrophoneCalibration>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<MicrophoneCalibration>::ok({.id = col.nextI64(),
                                                   .mic_id = col.nextI32(),
                                                   .sensitivity = col.nextF32(),
                                                   .calibrated_at = col.nextText()});
    });
}

auto SqliteMicrophoneRepository::insertCalibration(const MicrophoneCalibration& data)
    -> DbResult<MicrophoneCalibration> {
    return dbExecute<MicrophoneCalibration>("MicrophoneRepository::insertCalibration", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT INTO microphone_calibration (mic_id, sensitivity, calibrated_at) "
            "VALUES (?, ?, datetime('now'))");
        ParamBinder(stmt).i32(data.mic_id).real(data.sensitivity);
        stmt.exec();
        MicrophoneCalibration saved = data;
        saved.id = db_.getLastInsertRowid();
        return DbResult<MicrophoneCalibration>::ok(std::move(saved));
    });
}

}  // namespace sst::db
