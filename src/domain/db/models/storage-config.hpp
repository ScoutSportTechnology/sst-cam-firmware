#pragma once

#include <cstdint>
#include <string>

namespace sst::db {

// 0=logs | 1=recording | 2=snapshots | 3=thumbnails
enum class StorageType : uint8_t { kLogs = 0, kRecording = 1, kSnapshots = 2, kThumbnails = 3 };

// 0=txt | 1=mp4 | 2=jpg
enum class StorageFormat : uint8_t { kTxt = 0, kMp4 = 1, kJpg = 2 };

struct StorageConfig {
    int64_t user_id{0};
    StorageType type{StorageType::kLogs};
    bool enabled{true};
    StorageFormat format{StorageFormat::kTxt};
    std::string path;
};

}  // namespace sst::db
