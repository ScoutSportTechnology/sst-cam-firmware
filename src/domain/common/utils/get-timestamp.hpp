#pragma once

#include <chrono>

#include "domain/common/models/timestamp.hpp"

namespace sst::common::utils {
using namespace sst::common;
using namespace std::chrono;

static inline auto GetCurrentTimestamp() -> Timestamp { return steady_clock::now(); }
}  // namespace sst::common::utils