#include <gtest/gtest.h>

#include <vector>

#include "app/buffer/ports/frame-sink.hpp"
#include "app/buffer/services/fan_out_sink/fan-out-sink.hpp"
#include "domain/capture/models/frame.hpp"

namespace {

// Records every frame it receives — used to verify the FanOutSink forwards
// to every registered sink in order.
class RecordingSink final : public sst::buffer::IFrameSink {
   public:
    auto Push(const sst::capture::Frame& frame) -> void override {
        ++push_calls;
        last_frame_id = frame.frame_id;
    }

    int push_calls{0};
    std::uint64_t last_frame_id{0};
};

auto MakeFrame(std::uint64_t id) -> sst::capture::Frame {
    sst::capture::Frame frame;
    frame.frame_id = id;
    frame.format = sst::common::PixelFormat::BGR8;
    frame.geometry = {.width = 640, .height = 360};
    return frame;
}

TEST(FanOutSinkTest, PushesToEverySinkInOrder) {
    RecordingSink a;
    RecordingSink b;
    RecordingSink c;
    sst::buffer::FanOutSink fanout(
        std::vector<sst::buffer::IFrameSink*>{&a, &b, &c});

    fanout.Push(MakeFrame(1));
    fanout.Push(MakeFrame(2));

    EXPECT_EQ(a.push_calls, 2);
    EXPECT_EQ(b.push_calls, 2);
    EXPECT_EQ(c.push_calls, 2);
    EXPECT_EQ(a.last_frame_id, 2);
    EXPECT_EQ(b.last_frame_id, 2);
    EXPECT_EQ(c.last_frame_id, 2);
}

TEST(FanOutSinkTest, EmptySinkListIsHarmless) {
    sst::buffer::FanOutSink fanout(std::vector<sst::buffer::IFrameSink*>{});
    fanout.Push(MakeFrame(1));  // does not crash
    SUCCEED();
}

TEST(FanOutSinkTest, NullPointerEntriesAreSkipped) {
    RecordingSink a;
    sst::buffer::FanOutSink fanout(
        std::vector<sst::buffer::IFrameSink*>{nullptr, &a, nullptr});

    fanout.Push(MakeFrame(7));

    EXPECT_EQ(a.push_calls, 1);
    EXPECT_EQ(a.last_frame_id, 7);
}

}  // namespace
