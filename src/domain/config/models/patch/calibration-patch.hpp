#pragma once

#include "domain/config/models/calibration.hpp"

namespace sst::config {

inline auto apply_patch(CalibrationCameraDeviceData& mod,
                        const CalibrationCameraDeviceData& patch) -> void {
    if (patch.id) mod.id = patch.id;
    if (patch.intrinsic_matrix) mod.intrinsic_matrix = patch.intrinsic_matrix;
    if (patch.distortion_coefficients) mod.distortion_coefficients = patch.distortion_coefficients;
    if (patch.last_calibration_date) mod.last_calibration_date = patch.last_calibration_date;
}

inline auto apply_patch(CalibrationCamerasData& mod, const CalibrationCamerasData& patch) -> void {
    if (patch.exposure) mod.exposure = patch.exposure;
    if (patch.gain) mod.gain = patch.gain;
    if (patch.white_balance) mod.white_balance = patch.white_balance;
    if (patch.focus) mod.focus = patch.focus;
    if (patch.width) mod.width = patch.width;
    if (patch.height) mod.height = patch.height;
    if (patch.format) mod.format = patch.format;
    if (patch.fps) mod.fps = patch.fps;
    if (patch.device) mod.device = patch.device;
}

inline auto apply_patch(CalibrationMicrophoneDeviceData& mod,
                        const CalibrationMicrophoneDeviceData& patch) -> void {
    if (patch.id) mod.id = patch.id;
    if (patch.sensitivity) mod.sensitivity = patch.sensitivity;
    if (patch.last_calibration_date) mod.last_calibration_date = patch.last_calibration_date;
}

inline auto apply_patch(CalibrationMicrophonesData& mod,
                        const CalibrationMicrophonesData& patch) -> void {
    if (patch.noise_reduction) mod.noise_reduction = patch.noise_reduction;
    if (patch.device) mod.device = patch.device;
}

inline auto apply_patch(CalibrationData& mod, const CalibrationData& patch) -> void {
    if (patch.cameras) {
        if (!mod.cameras) mod.cameras = CalibrationCamerasData{};
        apply_patch(*mod.cameras, *patch.cameras);
    }
    if (patch.microphones) {
        if (!mod.microphones) mod.microphones = CalibrationMicrophonesData{};
        apply_patch(*mod.microphones, *patch.microphones);
    }
}

}  // namespace sst::config
