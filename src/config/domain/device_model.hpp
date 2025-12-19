#pragma once

#include <cstdint>
#include <string_view>

namespace sst::config::domain {
enum class DeviceModel : std::uint8_t { UNKNOWN, V1, V2, V3 };
inline auto FromStringDeviceModel(std::string_view model_str) -> DeviceModel {
    if (model_str == "sst_cam_v1" || model_str == "sst-cam-v1" || model_str == "SST-CAM-V1" || model_str == "SST_CAM_V1") {
        return DeviceModel::V1;
    }
    if (model_str == "sst_cam_v2" || model_str == "sst-cam-v2" || model_str == "SST-CAM-V2" || model_str == "SST_CAM_V2") {
        return DeviceModel::V2;
    }
    if (model_str == "sst_cam_v3" || model_str == "sst-cam-v3" || model_str == "SST-CAM-V3" || model_str == "SST_CAM_V3") {
        return DeviceModel::V3;
    }
    return DeviceModel::UNKNOWN;
}
inline auto ToStringDeviceModel(DeviceModel model) -> std::string_view {
    switch (model) {
        case DeviceModel::V1:
            return "sst_cam_v1";
        case DeviceModel::V2:
            return "sst_cam_v2";
        case DeviceModel::V3:
            return "sst_cam_v3";
        default:
            return "unknown";
    }
}
}  // namespace sst::config::domain