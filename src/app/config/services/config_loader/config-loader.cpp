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

using sst::config::adapters::JsonReaderAdapter;
using sst::config::adapters::JsonWriterAdapter;
using sst::config::domain::CalibrationData;
using sst::config::domain::DeviceData;
using sst::config::domain::ProfileData;
using sst::config::domain::StorageData;
using sst::config::domain::StreamData;

template <typename T>
static auto HasUser(const T& users, std::uint32_t user_id_to_search) -> bool {
    return std::any_of(users.begin(), users.end(),
                       [&](const auto& user) { return user.id == user_id_to_search; });
}

template <typename T>
static auto RetrieveUserById(const T& users, std::uint32_t user_id) -> const
    typename T::value_type* {
    auto userIterator = std::find_if(users.begin(), users.end(),
                                     [&](const auto& user) { return user.id == user_id; });
    return (userIterator == users.end()) ? nullptr : &*userIterator;
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
    using sst::config::adapters::JsonReaderAdapter;
    using sst::config::adapters::JsonWriterAdapter;
    const std::string ext = file_type_;
    if (file_type_ == "json") {
        usersAdapter_ =
            std::make_unique<JsonReaderAdapter<UsersConfig>>(MakePath(root_path_, "users", ext));
        profileAdapter_ =
            std::make_unique<JsonReaderAdapter<ProfileConfig>>(MakePath(root_path_, "profile", ext));
        deviceAdapter_ =
            std::make_unique<JsonReaderAdapter<DeviceConfig>>(MakePath(root_path_, "device", ext));
        storageAdapter_ =
            std::make_unique<JsonReaderAdapter<StorageConfig>>(MakePath(root_path_, "storage", ext));
        streamAdapter_ =
            std::make_unique<JsonReaderAdapter<StreamConfig>>(MakePath(root_path_, "stream", ext));
        calibrationAdapter_ = std::make_unique<JsonReaderAdapter<CalibrationConfig>>(
            MakePath(root_path_, "calibration", ext));
    } else {
        throw std::runtime_error("Unsupported config file type: " + file_type_);
    }
}

auto ConfigLoader::get() -> ConfigData {
    if (!usersAdapter_ || !profileAdapter_ || !deviceAdapter_ || !storageAdapter_ ||
        !streamAdapter_ || !calibrationAdapter_) {
        throw std::logic_error("ConfigLoader adapters not initialized");
    }
    const auto usersConfig = usersAdapter_->load();
    const auto profileConfig = profileAdapter_->load();
    const auto deviceConfig = deviceAdapter_->load();
    const auto storageConfig = storageAdapter_->load();
    const auto streamConfig = streamAdapter_->load();
    const auto calibrationConfig = calibrationAdapter_->load();

    if (fail("users", usersConfig) || fail("profile", profileConfig) ||
        fail("device", deviceConfig) || fail("storage", storageConfig) ||
        fail("stream", streamConfig) || fail("calibration", calibrationConfig)) {
        throw std::runtime_error("failed to load configuration files");
    }

    const bool user_ok = user_id_.has_value() && !usersConfig.data.users.empty() &&
                         HasUser(usersConfig.data.users, *user_id_);

    ConfigData out{};

    if (!user_ok) {
        out.device = deviceConfig.data.default_data;
        out.storage = storageConfig.data.default_data;
        out.stream = streamConfig.data.default_data;
        out.calibration = calibrationConfig.data.default_data;
        return out;
    }

    const ProfileData profileData =
        GetData<ProfileConfig, ProfileData>(profileConfig.data, user_id_);

    const bool device_modified = profileData.device.value_or(false);
    const bool storage_modified = profileData.storage.value_or(false);
    const bool stream_modified = profileData.stream.value_or(false);
    const bool calibration_modified = profileData.calibration.value_or(false);

    out.device = device_modified ? GetData<DeviceConfig, DeviceData>(deviceConfig.data, user_id_)
                                 : deviceConfig.data.default_data;

    out.storage = storage_modified
                      ? GetData<StorageConfig, StorageData>(storageConfig.data, user_id_)
                      : storageConfig.data.default_data;

    out.stream = stream_modified ? GetData<StreamConfig, StreamData>(streamConfig.data, user_id_)
                                 : streamConfig.data.default_data;

    out.calibration = calibration_modified ? GetData<CalibrationConfig, CalibrationData>(
                                                 calibrationConfig.data, user_id_)
                                           : calibrationConfig.data.default_data;

    return out;
}

}  // namespace sst::config::app
