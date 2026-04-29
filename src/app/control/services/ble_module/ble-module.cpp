#include "app/control/services/ble_module/ble-module.hpp"

#include <utility>

#include <spdlog/spdlog.h>

#include "domain/control/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::control {

BleModule::BleModule(std::unique_ptr<IBleTransport> transport)
    : transport_(std::move(transport)) {
    transport_->SetOnCommand([this](Command cmd) { OnCommand(std::move(cmd)); });
}

BleModule::~BleModule() {
    if (transport_ && transport_->IsRunning()) {
        transport_->Stop();
    }
}

auto BleModule::Register(std::shared_ptr<IController> controller) -> void {
    if (!controller) {
        spdlog::error("BleModule::Register: null controller ignored");
        return;
    }
    std::lock_guard lock(mtx_);
    auto id = std::string(controller->Id());
    if (id.empty()) {
        spdlog::error("BleModule::Register: controller with empty Id() ignored");
        return;
    }
    auto [it, inserted] = controllers_.emplace(std::move(id), std::move(controller));
    if (!inserted) {
        spdlog::warn("BleModule::Register: replacing existing controller id=\"{}\"",
                     it->first);
        it->second = std::move(controller);
    } else {
        spdlog::info("BleModule::Register: controller id=\"{}\"", it->first);
    }
}

auto BleModule::Start() -> void { transport_->Start(); }

auto BleModule::Stop() -> void { transport_->Stop(); }

auto BleModule::IsRunning() const -> bool { return transport_->IsRunning(); }

auto BleModule::OnCommand(Command cmd) -> void {
    spdlog::debug("BleModule received {}", cmd);

    std::shared_ptr<IController> controller;
    {
        std::lock_guard lock(mtx_);
        if (auto it = controllers_.find(cmd.route); it != controllers_.end()) {
            controller = it->second;
        }
    }

    CommandResult result;
    result.correlation_id = cmd.correlation_id;
    if (!controller) {
        spdlog::warn("BleModule: no controller registered for route=\"{}\"", cmd.route);
        result.status = CommandStatus::kNotFound;
    } else {
        result = controller->Handle(cmd);
        result.correlation_id = cmd.correlation_id;
    }

    transport_->SendResult(result);
}

}  // namespace sst::control
