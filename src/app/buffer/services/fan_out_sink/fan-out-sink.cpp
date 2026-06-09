#include "app/buffer/services/fan_out_sink/fan-out-sink.hpp"

#include <utility>

namespace sst::buffer {

FanOutSink::FanOutSink(std::vector<IFrameSink*> sinks) : sinks_(std::move(sinks)) {}

auto FanOutSink::Push(const sst::capture::Frame& frame) -> void {
    for (auto* sink : sinks_) {
        if (sink != nullptr) {
            sink->Push(frame);
        }
    }
}

}  // namespace sst::buffer
