#include "app/control/cli.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sst::control {

namespace {

constexpr const char* kUsage = R"(Usage: sst_cam_firmware [OPTIONS]

SST Camera Firmware - Jetson Orin Nano embedded runtime

Options:
  --version             Print version information and exit
  --capture-diagnostic  Run isolated 10-minute Argus check (requires SST_ENABLE_ARGUS_DIAGNOSTIC=1)
  -h, --help            Print this help message and exit

Without options, starts the firmware (normal systemd behavior).
)";

}  // namespace

auto ParseArgs(int argc, char** argv, const std::string& version,
               const std::string& git_sha) -> CliResult {
    CliResult result;

    if (argc == 1) {
        result.action = CliAction::kRunFirmware;
        return result;
    }

    if (argc == 2 && std::string(argv[1]) == "--capture-diagnostic") {
        const char* gate = std::getenv("SST_ENABLE_ARGUS_DIAGNOSTIC");
        if (gate == nullptr || std::string(gate) != "1") {
            result.action = CliAction::kError;
            result.error =
                "sst_cam_firmware: capture diagnostic disabled (set "
                "SST_ENABLE_ARGUS_DIAGNOSTIC=1)\n";
            result.exit_code = 2;
            return result;
        }
        result.action = CliAction::kCaptureDiagnostic;
        return result;
    }
    for (int i = 1; i < argc; ++i) {
        if (argc > 2 && std::string(argv[i]) == "--capture-diagnostic") {
            result.action = CliAction::kError;
            result.error = "sst_cam_firmware: --capture-diagnostic must be used alone\n";
            result.exit_code = 2;
            return result;
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--version") {
            result.action = CliAction::kPrintVersion;
            result.output = "sst_cam_firmware ";
            result.output += version;
            result.output += " (git ";
            result.output += git_sha;
            result.output += ")";
            result.exit_code = 0;
            return result;
        }

        if (arg == "-h" || arg == "--help") {
            result.action = CliAction::kPrintHelp;
            result.output = kUsage;
            result.exit_code = 0;
            return result;
        }

        // Unknown argument
        result.action = CliAction::kError;
        result.error = "sst_cam_firmware: unrecognized option '" + arg + "'\n" + kUsage;
        result.exit_code = 2;
        return result;
    }

    result.action = CliAction::kRunFirmware;
    return result;
}

}  // namespace sst::control