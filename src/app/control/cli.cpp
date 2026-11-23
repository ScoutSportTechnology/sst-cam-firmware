#include "app/control/cli.hpp"

#include <charconv>
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
  --capture-diagnostic  Run isolated Argus check (gate SST_ENABLE_ARGUS_DIAGNOSTIC=1; optional SST_ARGUS_DIAGNOSTIC_START=dark|bright, SST_ARGUS_DIAGNOSTIC_SECONDS=1..600, SST_ARGUS_DIAGNOSTIC_TARGET=0.30 test-only)
  -h, --help            Print this help message and exit

Without options, starts the firmware (normal systemd behavior).
)";

auto ParseDiagnosticSeconds(CliResult& result) -> bool {
    const char* seconds = std::getenv("SST_ARGUS_DIAGNOSTIC_SECONDS");
    if (seconds == nullptr) {
        return true;
    }
    const std::string value(seconds);
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed < 1 ||
        parsed > kDefaultDiagnosticSeconds) {
        result.action = CliAction::kError;
        result.error = "sst_cam_firmware: diagnostic seconds must be 1..600\n";
        result.exit_code = 2;
        return false;
    }
    result.diagnostic_seconds = parsed;
    return true;
}

auto ParseDiagnosticTarget(CliResult& result) -> bool {
    const char* target = std::getenv("SST_ARGUS_DIAGNOSTIC_TARGET");
    if (target == nullptr) {
        return true;
    }
    if (std::string(target) != "0.30") {
        result.action = CliAction::kError;
        result.error = "sst_cam_firmware: diagnostic target must be 0.30\n";
        result.exit_code = 2;
        return false;
    }
    result.diagnostic_target = kApprovedDiagnosticTarget;
    return true;
}

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
        const char* start = std::getenv("SST_ARGUS_DIAGNOSTIC_START");
        if (start != nullptr && std::string(start) != "dark" && std::string(start) != "bright") {
            result.action = CliAction::kError;
            result.error = "sst_cam_firmware: invalid diagnostic start (dark or bright)\n";
            result.exit_code = 2;
            return result;
        }
        if (!ParseDiagnosticSeconds(result) || !ParseDiagnosticTarget(result)) {
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