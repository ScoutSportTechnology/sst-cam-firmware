#pragma once

#include <cstdint>
#include <string>

namespace sst::network {

// Disk-enumerated recording metadata for ListRecordings. The recording_id is
// the MP4 file stem (the match UUID); sport/teams are filled from the in-memory
// session when available.
struct RecordingSummary {
    std::string recording_id;
    std::uint64_t size_bytes{0};
    std::uint64_t started_at_unix{0};
    std::uint64_t duration_s{0};
    std::string thumbnail_id;
    std::string sport;
    std::string teams;
};

}  // namespace sst::network
