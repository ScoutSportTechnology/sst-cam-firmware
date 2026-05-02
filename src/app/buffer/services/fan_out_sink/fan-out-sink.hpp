#pragma once

#include <vector>

#include "app/buffer/ports/frame-sink.hpp"

namespace sst::buffer {

// Aggregates N IFrameSinks behind a single IFrameSink. Push() forwards the
// frame to each underlying sink in registration order. Sinks are stored as
// non-owning pointers — the caller (typically main.cpp) keeps the concrete
// services alive for the FanOutSink's lifetime.
//
// Lifecycle: sinks are passed at construction and not mutable afterward, so
// no internal locking is needed; each downstream sink is responsible for its
// own thread-safety.
class FanOutSink final : public IFrameSink {
   public:
    explicit FanOutSink(std::vector<IFrameSink*> sinks);
    ~FanOutSink() override = default;

    FanOutSink(const FanOutSink&) = delete;
    auto operator=(const FanOutSink&) -> FanOutSink& = delete;
    FanOutSink(FanOutSink&&) = delete;
    auto operator=(FanOutSink&&) -> FanOutSink& = delete;

    auto Push(const sst::capture::Frame& frame) -> void override;

   private:
    std::vector<IFrameSink*> sinks_;
};

}  // namespace sst::buffer
