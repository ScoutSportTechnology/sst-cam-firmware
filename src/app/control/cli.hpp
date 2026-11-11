#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sst::control {

enum class CliAction : std::uint8_t {
  kRunFirmware,
  kPrintVersion,
  kPrintHelp,
  kError,
};

struct CliResult {
  CliAction action = CliAction::kRunFirmware;
  std::string output;   // For --version/--help
  std::string error;    // For error messages
  int exit_code = 0;
};

auto ParseArgs(int argc, char** argv,
               const std::string& version,
               const std::string& git_sha) -> CliResult;

}  // namespace sst::control