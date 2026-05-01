#include "app/control/services/controllers/network.controller.hpp"

#include <string>
#include <string_view>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

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

NetworkController::NetworkController(WifiModule& wifi, sst::db::INetworkRepository& network_repo,
                                     int64_t user_id)
    : wifi_(wifi), network_repo_(network_repo), user_id_(user_id) {}

auto NetworkController::Handle(const Command& cmd) -> CommandResult {
    const auto verb = AsStringView(cmd.payload);
    spdlog::info("NetworkController: verb=\"{}\"", verb);

    CommandResult result;

    if (verb == "bootstrap_p2p") {
        auto creds_row = network_repo_.getWifiDirect(user_id_);
        if (!creds_row.success) {
            spdlog::error(
                "NetworkController: no config_wifi_direct row for user_id={} "
                "(seeding failed?)",
                user_id_);
            result.status = CommandStatus::kInternal;
            return result;
        }

        WifiCredentials creds{
            .ssid = creds_row.data.ssid,
            .passphrase = creds_row.data.passphrase,
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
