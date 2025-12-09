#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace sst::config::adapters {

using nlohmann::json;

template <typename T>
class JsonAdapter {
   public:
    explicit JsonAdapter(std::filesystem::path path) : path_(std::move(path)) {}

    auto load(T& loadedConfig, std::string& error) -> bool {
        try {
            std::ifstream inputFileStream(path_);
            if (!inputFileStream) {
                error = "Cannot open config file: " + path_.string();
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

    auto save(const T& modifiedConfig, std::string& error) -> bool {
        try {
            std::ofstream out(path_);
            if (!out) {
                error = "Cannot open config file for writing: " + path_.string();
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
    std::filesystem::path path_;
};

}  // namespace sst::config::adapters
