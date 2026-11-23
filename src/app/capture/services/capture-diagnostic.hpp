#pragma once

#include <chrono>
#include <memory>
#include <ostream>

#include "adapters/capture/frame/argus/direct-capture.hpp"

namespace sst::capture {
// An operator-only capture check; never called by normal firmware startup.
auto RunCaptureDiagnostic(std::shared_ptr<argus::Hardware> hardware,
                          std::chrono::steady_clock::duration duration, std::ostream& output,
                          float target_luma = argus::kDefaultTargetLuma) -> int;
}  // namespace sst::capture
