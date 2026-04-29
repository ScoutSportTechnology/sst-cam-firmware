#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sst::control {

enum class CommandStatus : std::uint8_t {
    kOk = 0,
    kInvalidArgument = 1,
    kNotFound = 2,
    kFailedPrecondition = 3,
    kUnimplemented = 4,
    kInternal = 5,
};

struct CommandResult {
    CommandStatus status{CommandStatus::kOk};
    std::vector<std::byte> payload;
    std::uint64_t correlation_id{0};
};

}  // namespace sst::control
