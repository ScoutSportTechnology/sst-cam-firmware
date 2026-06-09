#pragma once

#include <cstdint>
#include <string>

namespace sst::network {

// A short-lived bearer token scoped to exactly one recording, minted in
// response to DownloadRequest and gating one HTTP download (KTD7).
struct DownloadToken {
    std::string recording_id;
    std::string token;
    std::uint64_t expires_at_unix{0};
};

}  // namespace sst::network
