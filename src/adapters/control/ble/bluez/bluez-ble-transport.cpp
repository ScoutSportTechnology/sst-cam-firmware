#include "adapters/control/ble/bluez/bluez-ble-transport.hpp"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "domain/control/models/bootstrap-config.hpp"
#include "domain/control/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::adapters::control {

namespace {

constexpr const char* kBluezBus = "org.bluez";
constexpr const char* kIfaceGattManager = "org.bluez.GattManager1";
constexpr const char* kIfaceLeAdvManager = "org.bluez.LEAdvertisingManager1";
constexpr const char* kIfaceLeAdv = "org.bluez.LEAdvertisement1";

// Wire format (provisional, replaced when .proto schema lands):
//   Command := u32_le(route_len) | route | u64_le(corr_id) | payload
//   Result  := u8(status)        | u64_le(corr_id) | payload
auto ParseCommand(const std::vector<std::uint8_t>& bytes) -> std::optional<sst::control::Command> {
    if (bytes.size() < 4) {
        return std::nullopt;
    }
    std::uint32_t route_len = 0;
    std::memcpy(&route_len, bytes.data(), 4);
    if (4 + route_len + 8 > bytes.size()) {
        return std::nullopt;
    }
    sst::control::Command cmd;
    cmd.route.assign(reinterpret_cast<const char*>(bytes.data() + 4), route_len);
    std::memcpy(&cmd.correlation_id, bytes.data() + 4 + route_len, 8);
    const std::size_t header = 4 + route_len + 8;
    cmd.payload.resize(bytes.size() - header);
    for (std::size_t i = 0; i < cmd.payload.size(); ++i) {
        cmd.payload[i] = static_cast<std::byte>(bytes[header + i]);
    }
    return cmd;
}

auto SerializeResult(const sst::control::CommandResult& r) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> out;
    out.reserve(1 + 8 + r.payload.size());
    out.push_back(static_cast<std::uint8_t>(r.status));
    const auto corr = r.correlation_id;
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((corr >> (i * 8)) & 0xFFU));
    }
    for (auto b : r.payload) {
        out.push_back(static_cast<std::uint8_t>(b));
    }
    return out;
}

}  // namespace

BluezBleTransport::BluezBleTransport(std::string adapter_path)
    : adapter_path_(std::move(adapter_path)),
      app_root_path_("/com/sst/cam/gatt"),
      adv_path_("/com/sst/cam/adv") {}

BluezBleTransport::~BluezBleTransport() {
    if (running_) {
        Stop();
    }
}

auto BluezBleTransport::IsRunning() const -> bool { return running_; }

auto BluezBleTransport::SetOnCommand(CommandHandler handler) -> void {
    std::lock_guard lock(mtx_);
    on_command_ = std::move(handler);
}

auto BluezBleTransport::OnRawCommand(std::vector<std::uint8_t> bytes) -> void {
    auto cmd = ParseCommand(bytes);
    if (!cmd) {
        spdlog::warn("BluezBleTransport: malformed command of {} bytes — dropping",
                     bytes.size());
        return;
    }
    spdlog::debug("BluezBleTransport received {}", *cmd);

    CommandHandler handler;
    {
        std::lock_guard lock(mtx_);
        handler = on_command_;
    }
    if (handler) {
        handler(std::move(*cmd));
    } else {
        spdlog::warn("BluezBleTransport: no command handler set");
    }
}

auto BluezBleTransport::BuildAdvertisement() -> void {
    adv_obj_ = sdbus::createObject(*connection_, adv_path_);

    adv_obj_->registerProperty("Type")
        .onInterface(kIfaceLeAdv)
        .withGetter([] { return std::string{"peripheral"}; });
    adv_obj_->registerProperty("ServiceUUIDs")
        .onInterface(kIfaceLeAdv)
        .withGetter([] {
            return std::vector<std::string>{
                std::string{sst::control::BootstrapDefaults::kGattServiceUuid}};
        });
    adv_obj_->registerProperty("LocalName")
        .onInterface(kIfaceLeAdv)
        .withGetter([] {
            return std::string{sst::control::BootstrapDefaults::kBleAdvertisedName};
        });
    adv_obj_->registerProperty("Includes")
        .onInterface(kIfaceLeAdv)
        .withGetter([] { return std::vector<std::string>{"tx-power"}; });

    adv_obj_->registerMethod("Release")
        .onInterface(kIfaceLeAdv)
        .implementedAs([] { spdlog::info("LEAdvertisement1.Release called"); });

    adv_obj_->finishRegistration();
}

auto BluezBleTransport::Start() -> void {
    if (running_) {
        return;
    }

    try {
        connection_ = sdbus::createSystemBusConnection();
    } catch (const sdbus::Error& e) {
        spdlog::error("BluezBleTransport: createSystemBusConnection failed: {}",
                      e.what());
        return;
    }

    try {
        gatt_app_ = std::make_unique<GattApplication>(
            *connection_, app_root_path_,
            std::string{sst::control::BootstrapDefaults::kGattServiceUuid},
            std::string{sst::control::BootstrapDefaults::kGattCommandCharUuid},
            std::string{sst::control::BootstrapDefaults::kGattResponseCharUuid},
            [this](std::vector<std::uint8_t> bytes) { OnRawCommand(std::move(bytes)); });

        BuildAdvertisement();

        auto adv_proxy = sdbus::createProxy(*connection_, kBluezBus, adapter_path_);
        adv_proxy->callMethod("RegisterAdvertisement")
            .onInterface(kIfaceLeAdvManager)
            .withArguments(sdbus::ObjectPath{adv_path_},
                           std::map<std::string, sdbus::Variant>{});
        advertisement_registered_ = true;

        auto gatt_proxy = sdbus::createProxy(*connection_, kBluezBus, adapter_path_);
        gatt_proxy->callMethod("RegisterApplication")
            .onInterface(kIfaceGattManager)
            .withArguments(sdbus::ObjectPath{app_root_path_},
                           std::map<std::string, sdbus::Variant>{});
        application_registered_ = true;

        connection_->enterEventLoopAsync();
        running_ = true;

        spdlog::info(
            "BluezBleTransport: advertising as \"{}\" with service {} on adapter {}",
            sst::control::BootstrapDefaults::kBleAdvertisedName,
            sst::control::BootstrapDefaults::kGattServiceUuid, adapter_path_);
    } catch (const sdbus::Error& e) {
        spdlog::error("BluezBleTransport::Start failed: {}: {}", e.getName(), e.getMessage());
        Stop();
    }
}

auto BluezBleTransport::Stop() -> void {
    if (!running_ && !advertisement_registered_ && !application_registered_ &&
        !connection_) {
        return;
    }
    spdlog::info("BluezBleTransport::Stop");

    if (connection_ && (advertisement_registered_ || application_registered_)) {
        try {
            auto proxy = sdbus::createProxy(*connection_, kBluezBus, adapter_path_);
            if (advertisement_registered_) {
                proxy->callMethod("UnregisterAdvertisement")
                    .onInterface(kIfaceLeAdvManager)
                    .withArguments(sdbus::ObjectPath{adv_path_});
                advertisement_registered_ = false;
            }
            if (application_registered_) {
                proxy->callMethod("UnregisterApplication")
                    .onInterface(kIfaceGattManager)
                    .withArguments(sdbus::ObjectPath{app_root_path_});
                application_registered_ = false;
            }
        } catch (const sdbus::Error& e) {
            spdlog::warn("BluezBleTransport: unregister failed: {}", e.what());
        }
    }

    if (connection_) {
        try {
            connection_->leaveEventLoop();
        } catch (const sdbus::Error&) {
        }
    }

    adv_obj_.reset();
    gatt_app_.reset();
    connection_.reset();
    running_ = false;
}

auto BluezBleTransport::SendResult(const sst::control::CommandResult& result) -> void {
    if (!gatt_app_) {
        spdlog::warn("BluezBleTransport::SendResult: GATT app not active");
        return;
    }
    spdlog::debug("BluezBleTransport sending {}", result);
    gatt_app_->SendNotification(SerializeResult(result));
}

}  // namespace sst::adapters::control
