#pragma once

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

#include "config/adapters/json/serde/_serde.hpp"  // IWYU pragma: keep
#include "config/ports/config_file_adapter.hpp"

namespace sst::config::adapters {

using nlohmann::json;
using sst::config::ports::ConfigReturn;
using sst::config::ports::IConfigFileAdapter;

template <typename T>
class JsonAdapter : public IConfigFileAdapter<T> {
   public:
    explicit JsonAdapter(std::filesystem::path full_path) : full_path_{std::move(full_path)} {}

    auto load() -> ConfigReturn<T> override {
        try {
            spdlog::debug("Loading JSON config from: {}", full_path_.string());

            std::ifstream inputFileStream(full_path_, std::ios::in);
            if (!inputFileStream.is_open()) {
                spdlog::error("Cannot open config file: {}", full_path_.string());
                return ConfigReturn<T>::fail();
            }

            json jsonData;
            inputFileStream >> jsonData;

            T loadedConfig = jsonData.get<T>();
            spdlog::debug("Loaded JSON config OK: {}", full_path_.string());

            return ConfigReturn<T>::ok(std::move(loadedConfig));

        } catch (const json::exception& ex) {
            spdlog::error("Failed to parse JSON config: {} (file: {})", ex.what(),
                          full_path_.string());
            return ConfigReturn<T>::fail();
        } catch (const std::exception& ex) {
            spdlog::error("Failed to load JSON config: {} (file: {})", ex.what(),
                          full_path_.string());
            return ConfigReturn<T>::fail();
        }
    }

    auto save(const T& modifiedConfig) -> ConfigReturn<T> override {
        try {
            spdlog::info("Saving JSON config to: {}", full_path_.string());

            auto loaded = load();
            if (!loaded.success) {
                spdlog::error("Could not load existing config file, proceeding to save new one.");
                return ConfigReturn<T>::fail();
            }

            json jsonData = loaded.data;
            jsonData["users"] = modifiedConfig.users;

            std::ofstream outputFileStream(full_path_, std::ios::out | std::ios::trunc);
            if (!outputFileStream.is_open()) {
                spdlog::error("Cannot open config file for writing: {}", full_path_.string());
                return ConfigReturn<T>::fail();
            }

            outputFileStream << jsonData.dump(2) << '\n';

            if (!outputFileStream.good()) {
                spdlog::error("Failed while writing config file: {}", full_path_.string());
                return ConfigReturn<T>::fail();
            }

            spdlog::debug("Saved JSON config OK: {}", full_path_.string());

            T updatedConfig = jsonData.get<T>();
            return ConfigReturn<T>::ok(std::move(updatedConfig));

        } catch (const std::exception& ex) {
            spdlog::error("Failed to save JSON config: {} (file: {})", ex.what(),
                          full_path_.string());
            return ConfigReturn<T>::fail();
        }
    }

    auto reset() -> ConfigReturn<T> override {
        try {
            spdlog::info("Reset JSON config to: {}", full_path_.string());

            T modifiedConfig{};

            auto resetConfig = save(modifiedConfig);
            if (!resetConfig.success) {
                spdlog::error("Could not reset config file: {}", full_path_.string());
                return ConfigReturn<T>::fail();
            }

            spdlog::debug("Reset JSON config OK: {}", full_path_.string());
            return resetConfig;

        } catch (const std::exception& ex) {
            spdlog::error("Reset failed: {} (file: {})", ex.what(), full_path_.string());
            return ConfigReturn<T>::fail();
        }
    }

   private:
    std::filesystem::path full_path_;
};

}  // namespace sst::config::adapters
