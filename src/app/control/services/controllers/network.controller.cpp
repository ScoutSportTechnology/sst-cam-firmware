#include "app/control/services/controllers/network.controller.hpp"

#include <string>
#include <string_view>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "domain/control/models/bootstrap-config.hpp"

namespace sst::control {

namespace {

auto AsStringView(const std::vector<std::byte>& bytes) -> std::string_view {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

auto MakePayload(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> out(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<std::byte>(text[i]);
    }
    return out;
}

}  // namespace

NetworkController::NetworkController(WifiModule& wifi) : wifi_(wifi) {}

auto NetworkController::Handle(const Command& cmd) -> CommandResult {
    const auto verb = AsStringView(cmd.payload);
    spdlog::info("NetworkController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "bootstrap_p2p") {
        WifiCredentials creds{
            .ssid = std::string(BootstrapDefaults::kP2pSsid),
            .passphrase = std::string(BootstrapDefaults::kP2pPassphrase),
        };
        if (!wifi_.StartP2pGroupOwner(creds)) {
            spdlog::error("NetworkController: StartP2pGroupOwner failed");
            result.status = CommandStatus::kInternal;
            return result;
        }
        result.status = CommandStatus::kOk;
        result.payload = MakePayload(fmt::format("{}\n{}", creds.ssid, creds.passphrase));
        return result;
    }

    if (verb == "stop") {
        wifi_.Stop();
        result.status = CommandStatus::kOk;
        return result;
    }

    spdlog::warn("NetworkController: unknown verb \"{}\"", verb);
    result.status = CommandStatus::kInvalidArgument;
    return result;
}

}  // namespace sst::control
