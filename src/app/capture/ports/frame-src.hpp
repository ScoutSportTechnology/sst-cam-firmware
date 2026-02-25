#pragma once

#include <optional>

#include "domain/capture/models/frame.hpp"
#include "domain/config/models/config_data.hpp"

namespace sst::capture::ports {

using sst::capture::domain::Frame;
using sst::config::domain::ConfigData;

class ICapture {
   public:
    virtual ~ICapture() = default;

    virtual auto Start() -> void = 0;
    virtual auto Stop() -> void = 0;
    virtual auto Restart() -> void {
        Stop();
        Start();
    };
    virtual auto IsRunning() const -> bool = 0;
    virtual auto Capture() -> std::optional<Frame> = 0;
};

}  // namespace sst::capture::ports