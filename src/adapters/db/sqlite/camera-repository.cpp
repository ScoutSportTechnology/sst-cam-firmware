#include "adapters/db/sqlite/camera-repository.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace sst::db {

SqliteCameraRepository::SqliteCameraRepository(DbConnection& conn) : db_(conn.db()) {}

auto SqliteCameraRepository::getConfig(int64_t user_id) -> DbResult<CameraConfig> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, exposure, gain, white_balance, focus, width, height, format, fps "
            "FROM camera_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<CameraConfig>::fail();
        }
        CameraConfig cfg;
        int col = 0;
        cfg.user_id = stmt.getColumn(col++).getInt64();
        cfg.exposure = stmt.getColumn(col++).getInt();
        cfg.gain = static_cast<float>(stmt.getColumn(col++).getDouble());
        cfg.white_balance = static_cast<CameraWhiteBalance>(stmt.getColumn(col++).getInt());
        cfg.focus = static_cast<CameraFocus>(stmt.getColumn(col++).getInt());
        cfg.width = stmt.getColumn(col++).getInt();
        cfg.height = stmt.getColumn(col++).getInt();
        cfg.format = static_cast<CameraFormat>(stmt.getColumn(col++).getInt());
        cfg.fps = stmt.getColumn(col).getInt();
        return DbResult<CameraConfig>::ok(cfg);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("CameraRepository::getConfig failed: {}", ex.what());
        return DbResult<CameraConfig>::fail();
    }
}

auto SqliteCameraRepository::saveConfig(const CameraConfig& data) -> DbResult<CameraConfig> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO camera_config "
            "(user_id, exposure, gain, white_balance, focus, width, height, format, fps) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        int param = 1;
        stmt.bind(param++, data.user_id);
        stmt.bind(param++, data.exposure);
        stmt.bind(param++, static_cast<double>(data.gain));
        stmt.bind(param++, static_cast<int>(data.white_balance));
        stmt.bind(param++, static_cast<int>(data.focus));
        stmt.bind(param++, data.width);
        stmt.bind(param++, data.height);
        stmt.bind(param++, static_cast<int>(data.format));
        stmt.bind(param, data.fps);
        stmt.exec();
        return DbResult<CameraConfig>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("CameraRepository::saveConfig failed: {}", ex.what());
        return DbResult<CameraConfig>::fail();
    }
}

auto SqliteCameraRepository::getLatestCalibration(int32_t camera_id)
    -> DbResult<CameraCalibration> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT id, camera_id, intrinsic_matrix, distortion_coefficients, calibrated_at "
            "FROM camera_calibration WHERE camera_id = ? "
            "ORDER BY calibrated_at DESC LIMIT 1");
        stmt.bind(1, camera_id);
        if (!stmt.executeStep()) {
            return DbResult<CameraCalibration>::fail();
        }
        CameraCalibration cal;
        int col = 0;
        cal.id = stmt.getColumn(col++).getInt64();
        cal.camera_id = stmt.getColumn(col++).getInt();
        auto intrinsicJson = nlohmann::json::parse(stmt.getColumn(col++).getText());
        auto distortionJson = nlohmann::json::parse(stmt.getColumn(col++).getText());
        cal.intrinsic_matrix = intrinsicJson.get<std::array<float, CameraCalibration::kIntrinsicMatrixSize>>();
        cal.distortion_coefficients = distortionJson.get<std::array<float, CameraCalibration::kDistortionCoefficientsSize>>();
        cal.calibrated_at = stmt.getColumn(col).getText();
        return DbResult<CameraCalibration>::ok(std::move(cal));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("CameraRepository::getLatestCalibration failed: {}", ex.what());
        return DbResult<CameraCalibration>::fail();
    } catch (const nlohmann::json::exception& ex) {
        spdlog::error("CameraRepository::getLatestCalibration JSON parse failed: {}", ex.what());
        return DbResult<CameraCalibration>::fail();
    }
}

auto SqliteCameraRepository::insertCalibration(const CameraCalibration& data)
    -> DbResult<CameraCalibration> {
    try {
        auto intrinsicJson = nlohmann::json(data.intrinsic_matrix).dump();
        auto distortionJson = nlohmann::json(data.distortion_coefficients).dump();
        SQLite::Statement stmt(
            db_,
            "INSERT INTO camera_calibration "
            "(camera_id, intrinsic_matrix, distortion_coefficients, calibrated_at) "
            "VALUES (?, ?, ?, datetime('now'))");
        int param = 1;
        stmt.bind(param++, data.camera_id);
        stmt.bind(param++, intrinsicJson);
        stmt.bind(param, distortionJson);
        stmt.exec();
        CameraCalibration saved = data;
        saved.id = db_.getLastInsertRowid();
        return DbResult<CameraCalibration>::ok(std::move(saved));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("CameraRepository::insertCalibration failed: {}", ex.what());
        return DbResult<CameraCalibration>::fail();
    }
}

}  // namespace sst::db
