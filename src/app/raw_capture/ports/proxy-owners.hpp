#pragma once

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include "app/raw_capture/ports/raw-capture-sink.hpp"

namespace sst::raw_capture {

class ProxyOwners {
   public:
    enum class Owner : std::uint8_t { kRecording, kStreaming };

    explicit ProxyOwners(IRawCaptureSink& sink) : sink_(sink) {}

    auto Start(Owner owner, const std::string& match_uuid,
               const std::filesystem::path& video_output_path) -> bool {
        std::lock_guard lock(mtx_);
        const auto dir =
            video_output_path.has_extension() ? video_output_path.parent_path() : video_output_path;
        const auto match_dir = dir.has_filename() ? dir : dir.parent_path();
        if (!ValidMatchUuid(match_uuid) || match_dir.filename() != match_uuid ||
            (video_output_path.has_extension() && video_output_path.stem() != match_uuid) ||
            ((recording_ || streaming_) && group_ != match_uuid)) {
            return false;
        }
        if (owner == Owner::kRecording ? recording_ : streaming_) {
            return true;
        }
        if (!recording_ && !streaming_) {
            if (!sink_.Start(match_uuid, match_dir)) {
                return false;
            }
            group_ = match_uuid;
        }
        (owner == Owner::kRecording ? recording_ : streaming_) = true;
        return true;
    }

    auto Stop(Owner owner) -> void {
        std::lock_guard lock(mtx_);
        auto& active = owner == Owner::kRecording ? recording_ : streaming_;
        if (!active) {
            return;
        }
        active = false;
        if (!recording_ && !streaming_) {
            sink_.Stop();
            group_.clear();
        }
    }

    auto StopAll() -> void {
        std::lock_guard lock(mtx_);
        recording_ = false;
        streaming_ = false;
        if (sink_.IsCapturing()) {
            sink_.Stop();
        }
        group_.clear();
    }

   private:
    static auto ValidMatchUuid(const std::string& match_uuid) -> bool {
        constexpr std::string_view kUuidPattern = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
        if (match_uuid.size() != kUuidPattern.size()) {
            return false;
        }
        for (std::size_t index = 0; index < match_uuid.size(); ++index) {
            const bool hyphen = kUuidPattern[index] == '-';
            if (hyphen ? match_uuid[index] != '-'
                       : std::isxdigit(static_cast<unsigned char>(match_uuid[index])) == 0) {
                return false;
            }
        }
        return true;
    }

    IRawCaptureSink& sink_;
    std::mutex mtx_;
    bool recording_{false};
    bool streaming_{false};
    std::string group_;
};

}  // namespace sst::raw_capture
