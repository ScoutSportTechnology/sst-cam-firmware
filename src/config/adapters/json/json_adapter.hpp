#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/adapters/json/serde/_serde.hpp"  // IWYU pragma: keep
#include "config/ports/config_file_repository.hpp"

namespace sst::config::adapters {

using nlohmann::json;

template <typename T>
class JsonAdapter : public sst::config::ports::IConfigFileRepository<T> {
   public:
    explicit JsonAdapter(std::filesystem::path full_path) : full_path_{std::move(full_path)} {}

    auto load(T& loadedConfig) -> bool override {
        std::string error{"Error not set"};
        try {
            spdlog::debug("Loading JSON config from: {}", full_path_.string());

            std::ifstream inputFileStream(full_path_, std::ios::in);
            if (!inputFileStream.is_open()) {
                error = "Cannot open config file: " + full_path_.string();
                spdlog::error("{}", error);
                return false;
            }

            json jsonData;
            inputFileStream >> jsonData;

            loadedConfig = jsonData.get<T>();
            spdlog::debug("Loaded JSON config OK: {}", full_path_.string());
            return true;

        } catch (const json::exception& ex) {
            error = std::string{"Failed to parse JSON config: "} + ex.what() +
                    " (file: " + full_path_.string() + ")";
            spdlog::error("{}", error);
            return false;

        } catch (const std::exception& ex) {
            error = std::string{"Failed to load JSON config: "} + ex.what() +
                    " (file: " + full_path_.string() + ")";
            spdlog::error("{}", error);
            return false;
        }
    }

    auto save(const T& modifiedConfig) -> bool override {
        std::string error{"Error not set"};
        try {
            spdlog::info("Saving JSON config to: {}", full_path_.string());

            std::ofstream out(full_path_, std::ios::out | std::ios::trunc);
            if (!out.is_open()) {
                error = "Cannot open config file for writing: " + full_path_.string();
                spdlog::error("{}", error);
                return false;
            }

            json jsonData = modifiedConfig;
            out << jsonData.dump(4) << '\n';

            if (!out.good()) {
                error = "Failed while writing config file: " + full_path_.string();
                spdlog::error("{}", error);
                return false;
            }

            spdlog::debug("Saved JSON config OK: {}", full_path_.string());
            return true;

        } catch (const std::exception& ex) {
            error = std::string{"Failed to save JSON config: "} + ex.what() +
                    " (file: " + full_path_.string() + ")";
            spdlog::error("{}", error);
            return false;
        }
    }

    auto reset() -> bool override {
        try {
            spdlog::info("Resetting JSON config (clearing users) at: {}", full_path_.string());

            T cfg{};

            if (std::filesystem::exists(full_path_)) {
                if (!load(cfg)) {
                    spdlog::error("Reset failed: could not load existing file: {}",
                                  full_path_.string());
                    return false;
                }
            }

            if constexpr (requires(T jsonData) { jsonData.users.clear(); }) {
                cfg.users.clear();
            } else {
                spdlog::error("Reset not supported: type has no 'users' member to clear");
                return false;
            }

            return save(cfg);

        } catch (const std::exception& ex) {
            spdlog::error("Reset failed: {} (file: {})", ex.what(), full_path_.string());
            return false;
        }
    }

   private:
    std::filesystem::path full_path_;
};

}  // namespace sst::config::adapters
