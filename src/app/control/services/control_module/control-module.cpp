#include "app/control/services/control_module/control-module.hpp"

#include <utility>

#include <spdlog/spdlog.h>

namespace sst::control {

ControlModule::ControlModule(std::unique_ptr<IBleTransport> ble_transport,
                             std::unique_ptr<IWifiManager> wifi_manager)
    : ble_(std::move(ble_transport)), wifi_(std::move(wifi_manager)) {}

auto ControlModule::Start() -> void {
    spdlog::info("ControlModule::Start");
    ble_.Start();
}

auto ControlModule::Stop() -> void {
    spdlog::info("ControlModule::Stop");
    ble_.Stop();
    wifi_.Stop();
}

}  // namespace sst::control
