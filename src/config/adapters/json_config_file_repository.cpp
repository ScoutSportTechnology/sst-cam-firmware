#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/ports/config_file_repository.hpp"

namespace sst::config::adapters {
using nlohmann::json;
using sst::config::ports::ConfigFileRepository;

template <typename T>
class JsonConfigFileRepository : public ConfigFileRepository<T> {
   public:
    explicit JsonConfigFileRepository(const std::string& fileName)
        : fullPath_(basePath_ + fileName + ".json") {}

    bool load_config(T& loadedConfig, std::string& error) override {
        try {
            std::ifstream inputFileStream(fullPath_);
            if (!inputFileStream) {
                error = "Could not open file: " + fullPath_;
                return false;
            }

            json jsonObject;
            inputFileStream >> jsonObject;
            loadedConfig = jsonObject.get<T>();
            return true;

        } catch (const std::exception& e) {
            error = std::string("Error reading config file '") + fullPath_ + "': " + e.what();
            return false;
        }
    }

    bool save_config(const T& modifiedConfig, std::string& error) override {
        // ...
    }

   private:
    std::string basePath_ = "../../../config/";
    std::string fullPath_;
};

}  // namespace sst::config::adapters
