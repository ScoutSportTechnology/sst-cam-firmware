#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "domain/raw_capture/models/raw-capture-identity.hpp"

// On-disk naming convention for internal dual-camera proxy files, shared by
// the writer, download exclusion, and retention sweep.
//
// Layout:  raw__<capture_group_id>__cam<N>.mp4
//
// The `raw__` prefix marks internal training proxies beside the final MP4.
// The group is the match UUID; `__cam<N>` names the physical sensor. Each file
// is a small H.264 MP4 (854x480@15). UUIDs contain no `__` delimiters.
namespace sst::raw_capture::raw_capture_naming {

inline constexpr std::string_view kPrefix = "raw__";
inline constexpr std::string_view kCameraMarker = "__cam";
inline constexpr std::string_view kExtension = ".mp4";

// `id` names a capture identity; this is a public API referenced outside this
// module (proxy writer + retention), so the parameter name is part of the
// contract and stays as-is.
// NOLINTNEXTLINE(readability-identifier-length)
inline auto FileName(const RawCaptureIdentity& id) -> std::string {
    return std::string(kPrefix) + id.capture_group_id + std::string(kCameraMarker) +
           std::to_string(id.camera_index) + std::string(kExtension);
}

// Parse a raw-capture filename back into its identity. Returns std::nullopt for
// any name that is not a raw-capture file (e.g. an `.mp4` final recording), so
// retention can distinguish them from final recordings.
inline auto ParseFileName(std::string_view file_name) -> std::optional<RawCaptureIdentity> {
    if (file_name.substr(0, kPrefix.size()) != kPrefix) {
        return std::nullopt;
    }
    if (file_name.size() < kExtension.size() ||
        file_name.substr(file_name.size() - kExtension.size()) != kExtension) {
        return std::nullopt;
    }
    const auto marker_pos = file_name.rfind(kCameraMarker);
    if (marker_pos == std::string_view::npos || marker_pos <= kPrefix.size()) {
        return std::nullopt;
    }

    const auto group_start = kPrefix.size();
    const auto group = file_name.substr(group_start, marker_pos - group_start);
    const auto index_start = marker_pos + kCameraMarker.size();
    const auto index_len = file_name.size() - kExtension.size() - index_start;
    const auto index_str = file_name.substr(index_start, index_len);
    if (group.empty() || index_str.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kDecimalBase = 10;
    std::uint32_t camera_index = 0;
    for (const char digit : index_str) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
        camera_index = camera_index * kDecimalBase + static_cast<std::uint32_t>(digit - '0');
    }

    return RawCaptureIdentity{.capture_group_id = std::string(group), .camera_index = camera_index};
}

}  // namespace sst::raw_capture::raw_capture_naming
