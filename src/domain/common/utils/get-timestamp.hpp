#pragma once

#include <chrono>

#include "common/domain/timestamp.hpp"

namespace sst::common::utils {
using namespace sst::common::domain;
using namespace std::chrono;

static inline auto GetCurrentTimestamp() -> Timestamp { return steady_clock::now(); }
}  // namespace sst::common::utils