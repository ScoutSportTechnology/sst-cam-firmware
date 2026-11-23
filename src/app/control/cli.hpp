#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "adapters/capture/frame/argus/direct-capture.hpp"

namespace sst::control {

enum class CliAction : std::uint8_t {
    kRunFirmware,
    kPrintVersion,
    kPrintHelp,
    kCaptureDiagnostic,
    kError,
};

inline constexpr int kDefaultDiagnosticSeconds = 600;
inline constexpr float kApprovedDiagnosticTarget = 0.30F;

struct CliResult {
    CliAction action = CliAction::kRunFirmware;
    std::string output;  // For --version/--help
    std::string error;   // For error messages
    int exit_code = 0;
    int diagnostic_seconds = kDefaultDiagnosticSeconds;
    float diagnostic_target = sst::capture::argus::kDefaultTargetLuma;
};

auto ParseArgs(int argc, char** argv, const std::string& version,
               const std::string& git_sha) -> CliResult;

}  // namespace sst::control