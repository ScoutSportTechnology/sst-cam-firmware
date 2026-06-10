#pragma once

#include <cstdint>
#include <string>

namespace sst::network {

// Disk-enumerated recording metadata for ListRecordings. The recording_id is
// the file stem; sport/teams are filled from the in-memory session when
// available.
//
// For raw dual-camera capture files (U6), is_raw is true and camera_index +
// capture_group_id carry the identity parsed from the filename. The joint
// invariant from the proto contract holds: when is_raw is true BOTH
// camera_index and capture_group_id are set; for final recordings is_raw is
// false and the other two are unset (empty group id).
struct RecordingSummary {
    std::string recording_id;
    std::uint64_t size_bytes{0};
    std::uint64_t started_at_unix{0};
    std::uint64_t duration_s{0};
    std::string thumbnail_id;
    std::string sport;
    std::string teams;

    bool is_raw{false};
    std::uint32_t camera_index{0};
    std::string capture_group_id;
};

}  // namespace sst::network
