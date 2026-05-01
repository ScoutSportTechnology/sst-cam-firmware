#include "adapters/db/sqlite/camera-repository.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace sst::db {

SqliteCameraRepository::SqliteCameraRepository(DbConnection& conn) : SqliteRepositoryBase(conn) {}

auto SqliteCameraRepository::getConfig(int64_t user_id) -> DbResult<CameraConfig> {
    return dbExecute<CameraConfig>("CameraRepository::getConfig", [&] {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, exposure, gain, white_balance, focus, width, height, format, fps, "
            "event_clip_pre_seconds, event_clip_post_seconds "
            "FROM camera_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<CameraConfig>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<CameraConfig>::ok({.user_id = col.nextI64(),
                                          .exposure = col.nextI32(),
                                          .gain = col.nextF32(),
                                          .white_balance = col.nextEnum<CameraWhiteBalance>(),
                                          .focus = col.nextEnum<CameraFocus>(),
                                          .width = col.nextI32(),
                                          .height = col.nextI32(),
                                          .format = col.nextEnum<sst::common::PixelFormat>(),
                                          .fps = col.nextI32(),
                                          .event_clip_pre_seconds = col.nextI32(),
                                          .event_clip_post_seconds = col.nextI32()});
    });
}

auto SqliteCameraRepository::saveConfig(const CameraConfig& data) -> DbResult<CameraConfig> {
    return dbExecute<CameraConfig>("CameraRepository::saveConfig", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO camera_config "
            "(user_id, exposure, gain, white_balance, focus, width, height, format, fps, "
            " event_clip_pre_seconds, event_clip_post_seconds) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .i64(data.user_id)
            .i32(data.exposure)
            .real(data.gain)
            .asEnum(data.white_balance)
            .asEnum(data.focus)
            .i32(data.width)
            .i32(data.height)
            .asEnum(data.format)
            .i32(data.fps)
            .i32(data.event_clip_pre_seconds)
            .i32(data.event_clip_post_seconds);
        stmt.exec();
        return DbResult<CameraConfig>::ok(data);
    });
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
        ColumnReader col(stmt);
        CameraCalibration cal;
        cal.id = col.nextI64();
        cal.camera_id = col.nextI32();
        cal.intrinsic_matrix =
            nlohmann::json::parse(col.nextText())
                .get<std::array<float, CameraCalibration::kIntrinsicMatrixSize>>();
        cal.distortion_coefficients =
            nlohmann::json::parse(col.nextText())
                .get<std::array<float, CameraCalibration::kDistortionCoefficientsSize>>();
        cal.calibrated_at = col.nextText();
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
    return dbExecute<CameraCalibration>("CameraRepository::insertCalibration", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT INTO camera_calibration "
            "(camera_id, intrinsic_matrix, distortion_coefficients, calibrated_at) "
            "VALUES (?, ?, ?, datetime('now'))");
        ParamBinder(stmt)
            .i32(data.camera_id)
            .text(nlohmann::json(data.intrinsic_matrix).dump())
            .text(nlohmann::json(data.distortion_coefficients).dump());
        stmt.exec();
        CameraCalibration saved = data;
        saved.id = db_.getLastInsertRowid();
        return DbResult<CameraCalibration>::ok(std::move(saved));
    });
}

}  // namespace sst::db
