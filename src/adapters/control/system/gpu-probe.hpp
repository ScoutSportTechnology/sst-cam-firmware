#pragma once

namespace sst::adapters::control {

auto GpuAvailable() -> bool;
auto RunGpuProbe() -> int;

}  // namespace sst::adapters::control
