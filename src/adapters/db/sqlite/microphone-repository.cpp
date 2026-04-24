#include "adapters/db/sqlite/microphone-repository.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

SqliteMicrophoneRepository::SqliteMicrophoneRepository(DbConnection& conn) : db_(conn.db()) {}

auto SqliteMicrophoneRepository::getConfig(int64_t user_id) -> DbResult<MicrophoneConfig> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, noise_reduction FROM microphone_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<MicrophoneConfig>::fail();
        }
        return DbResult<MicrophoneConfig>::ok(
            {.user_id = stmt.getColumn(0).getInt64(),
             .noise_reduction = static_cast<bool>(stmt.getColumn(1).getInt())});
    } catch (const SQLite::Exception& ex) {
        spdlog::error("MicrophoneRepository::getConfig failed: {}", ex.what());
        return DbResult<MicrophoneConfig>::fail();
    }
}

auto SqliteMicrophoneRepository::saveConfig(const MicrophoneConfig& data)
    -> DbResult<MicrophoneConfig> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO microphone_config (user_id, noise_reduction) VALUES (?, ?)");
        stmt.bind(1, data.user_id);
        stmt.bind(2, static_cast<int>(data.noise_reduction));
        stmt.exec();
        return DbResult<MicrophoneConfig>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("MicrophoneRepository::saveConfig failed: {}", ex.what());
        return DbResult<MicrophoneConfig>::fail();
    }
}

auto SqliteMicrophoneRepository::getLatestCalibration(int32_t mic_id)
    -> DbResult<MicrophoneCalibration> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT id, mic_id, sensitivity, calibrated_at "
            "FROM microphone_calibration WHERE mic_id = ? "
            "ORDER BY calibrated_at DESC LIMIT 1");
        stmt.bind(1, mic_id);
        if (!stmt.executeStep()) {
            return DbResult<MicrophoneCalibration>::fail();
        }
        int col = 0;
        return DbResult<MicrophoneCalibration>::ok(
            {.id = stmt.getColumn(col++).getInt64(),
             .mic_id = stmt.getColumn(col++).getInt(),
             .sensitivity = static_cast<float>(stmt.getColumn(col++).getDouble()),
             .calibrated_at = stmt.getColumn(col).getText()});
    } catch (const SQLite::Exception& ex) {
        spdlog::error("MicrophoneRepository::getLatestCalibration failed: {}", ex.what());
        return DbResult<MicrophoneCalibration>::fail();
    }
}

auto SqliteMicrophoneRepository::insertCalibration(const MicrophoneCalibration& data)
    -> DbResult<MicrophoneCalibration> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT INTO microphone_calibration (mic_id, sensitivity, calibrated_at) "
            "VALUES (?, ?, datetime('now'))");
        stmt.bind(1, data.mic_id);
        stmt.bind(2, static_cast<double>(data.sensitivity));
        stmt.exec();
        MicrophoneCalibration saved = data;
        saved.id = db_.getLastInsertRowid();
        return DbResult<MicrophoneCalibration>::ok(std::move(saved));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("MicrophoneRepository::insertCalibration failed: {}", ex.what());
        return DbResult<MicrophoneCalibration>::fail();
    }
}

}  // namespace sst::db
