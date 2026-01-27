#pragma once

#include "domain/config/models/calibration.hpp"

namespace sst::config::domain {

inline auto apply_patch(CalibrationCameraData& modifiedData,
                        const CalibrationCameraData& defaultData) -> void {
    if (defaultData.id) {
        modifiedData.id = defaultData.id;
    }
    if (defaultData.exposure) {
        modifiedData.exposure = defaultData.exposure;
    }
    if (defaultData.gain) {
        modifiedData.gain = defaultData.gain;
    }
    if (defaultData.white_balance) {
        modifiedData.white_balance = defaultData.white_balance;
    }
    if (defaultData.focus) {
        modifiedData.focus = defaultData.focus;
    }
    if (defaultData.width) {
        modifiedData.width = defaultData.width;
    }
    if (defaultData.height) {
        modifiedData.height = defaultData.height;
    }
    if (defaultData.format) {
        modifiedData.format = defaultData.format;
    }
    if (defaultData.fps) {
        modifiedData.fps = defaultData.fps;
    }
    if (defaultData.intrinsic_matrix) {
        modifiedData.intrinsic_matrix = defaultData.intrinsic_matrix;
    }
    if (defaultData.distortion_coefficients) {
        modifiedData.distortion_coefficients = defaultData.distortion_coefficients;
    }
    if (defaultData.last_calibration_date) {
        modifiedData.last_calibration_date = defaultData.last_calibration_date;
    }
}

inline auto apply_patch(CalibrationMicrophoneData& modifiedData,
                        const CalibrationMicrophoneData& defaultData) -> void {
    if (defaultData.last_calibration_date) {
        modifiedData.last_calibration_date = defaultData.last_calibration_date;
    }
    if (defaultData.id) {
        modifiedData.id = defaultData.id;
    }
    if (defaultData.sensitivity) {
        modifiedData.sensitivity = defaultData.sensitivity;
    }
    if (defaultData.noise_reduction) {
        modifiedData.noise_reduction = defaultData.noise_reduction;
    }
}

inline auto apply_patch(CalibrationDevicesData& modifiedData,
                        const CalibrationDevicesData& defaultData) -> void {
    if (defaultData.camera) {
        modifiedData.camera = defaultData.camera;
    }
    if (defaultData.microphone) {
        modifiedData.microphone = defaultData.microphone;
    }
}

inline auto apply_patch(CalibrationData& modifiedData, const CalibrationData& defaultData) -> void {
    if (defaultData.devices) {
        if (!modifiedData.devices) {
            modifiedData.devices = CalibrationDevicesData{};
        }
        apply_patch(*modifiedData.devices, *defaultData.devices);
    }
}

}  // namespace sst::config::domain
