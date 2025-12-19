#pragma once

#include <chrono>

namespace sst::common::domain {
using Timestamp = std::chrono::steady_clock::time_point;
}