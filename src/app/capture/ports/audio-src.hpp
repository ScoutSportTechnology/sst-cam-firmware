#pragma once

#include <optional>

#include "domain/capture/models/audio.hpp"
#include "domain/config/models/config-data.hpp"

namespace sst::capture {

using sst::config::ConfigData;

class ICaptureAudio {
   public:
    virtual ~ICaptureAudio() = default;

    virtual auto Start() -> void = 0;
    virtual auto Stop() -> void = 0;
    virtual auto Restart() -> void {
        Stop();
        Start();
    };
    virtual auto IsRunning() const -> bool = 0;
    virtual auto Capture() -> std::optional<Audio> = 0;
};

}  // namespace sst::capture