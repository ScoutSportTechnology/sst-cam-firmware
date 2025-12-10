#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/domain/config_files_json.hpp"
#include "config/domain/profile_config_json.hpp"
#include "config/ports/config_file_repository.hpp"

namespace sst::config::adapters {

using nlohmann::json;

template <typename T>
class JsonAdapter : public sst::config::ports::IConfigFileRepository<T> {
   public:
    explicit JsonAdapter(std::string filename)
        : full_path_{std::filesystem::path("config") / (std::move(filename) + ".json")} {}

    auto load(T& loadedConfig, std::string& error) -> bool override {
        try {
            spdlog::info("Loading JSON config from: {}", full_path_.string());

            std::ifstream inputFileStream(full_path_);
            if (!inputFileStream) {
                error = "Cannot open config file: " + full_path_.string();
                return false;
            }

            json jsonObject;
            inputFileStream >> jsonObject;

            loadedConfig = jsonObject.get<T>();

            return true;
        } catch (const std::exception& ex) {
            error = std::string{"Failed to load JSON config: "} + ex.what();
            return false;
        }
    }

    auto save(const T& modifiedConfig, std::string& error) -> bool override {
        try {
            std::cout << "Saving JSON config to: " << full_path_.string() << '\n';

            std::ofstream out(full_path_);
            if (!out) {
                error = "Cannot open config file for writing: " + full_path_.string();
                return false;
            }

            json jsonObject = modifiedConfig;
            out << jsonObject.dump(4);
            return true;
        } catch (const std::exception& ex) {
            error = std::string{"Failed to save JSON config: "} + ex.what();
            return false;
        }
    }

   private:
    std::filesystem::path full_path_;
};

}  // namespace sst::config::adapters
