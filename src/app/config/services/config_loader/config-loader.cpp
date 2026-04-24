#include "./config-loader.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "adapters/config/reader/json/json.hpp"
#include "adapters/config/writer/json/json.hpp"
#include "domain/config/models/patch/_patch.hpp"  // IWYU pragma: keep

namespace sst::config::app {

using sst::config::CalibrationData;
using sst::config::DeviceData;
using sst::config::JsonReaderAdapter;

template <typename T>
static auto RetrieveUserById(const T& users, std::uint32_t user_id) -> const
    typename T::value_type* {
    auto it = std::find_if(users.begin(), users.end(),
                           [&](const auto& user) { return user.id == user_id; });
    return (it == users.end()) ? nullptr : &*it;
}

template <typename FileModel, typename DataModel>
static auto GetData(const FileModel& file, const std::optional<std::uint32_t>& userId)
    -> DataModel {
    DataModel out = file.default_data;
    if (!userId) {
        return out;
    }
    const auto* entry = RetrieveUserById(file.users, *userId);
    if (!entry) {
        return out;
    }
    apply_patch(out, entry->user_data);
    return out;
}

static auto MakePath(const std::string& root, const std::string& filename,
                     const std::string& extension) -> std::filesystem::path {
    return std::filesystem::path(root) / (filename + "." + extension);
}

template <typename R>
static auto fail(std::string_view name, const R& configurationReference) -> bool {
    if (!configurationReference.success) {
        spdlog::error("Config load failed: {}", name);
        return true;
    }
    return false;
}

ConfigLoader::ConfigLoader(std::string root_path, std::string file_type,
                           std::optional<std::uint32_t> user_id)
    : user_id_(user_id), root_path_(std::move(root_path)), file_type_(std::move(file_type)) {
    const std::string ext = file_type_;
    if (file_type_ == "json") {
        deviceAdapter_ = std::make_unique<JsonReaderAdapter<DeviceConfig>>(
            MakePath(root_path_, "device", ext));
        calibrationAdapter_ = std::make_unique<JsonReaderAdapter<CalibrationConfig>>(
            MakePath(root_path_, "calibration", ext));
    } else {
        throw std::runtime_error("Unsupported config file type: " + file_type_);
    }
}

auto ConfigLoader::get() -> ConfigData {
    if (!deviceAdapter_ || !calibrationAdapter_) {
        throw std::logic_error("ConfigLoader adapters not initialized");
    }

    const auto deviceConfig = deviceAdapter_->load();
    const auto calibrationConfig = calibrationAdapter_->load();

    if (fail("device", deviceConfig) || fail("calibration", calibrationConfig)) {
        throw std::runtime_error("failed to load configuration files");
    }

    ConfigData out{};
    out.device = GetData<DeviceConfig, DeviceData>(deviceConfig.data, user_id_);
    out.calibration = GetData<CalibrationConfig, CalibrationData>(calibrationConfig.data, user_id_);
    return out;
}

}  // namespace sst::config::app
