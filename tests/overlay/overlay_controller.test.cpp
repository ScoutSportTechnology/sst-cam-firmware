// Overlay push-on-change controller (U9, R18/R20). Pure — fake renderer + sink.

#include <gtest/gtest.h>

#include <cstdint>

#include "app/overlay/ports/overlay-renderer.hpp"
#include "app/overlay/ports/overlay-sink.hpp"
#include "app/overlay/services/overlay_controller/overlay-controller.hpp"
#include "app/overlay/services/overlay_scene/overlay-scene.hpp"
#include "domain/overlay/models/overlay-enums.hpp"
#include "domain/overlay/models/overlay-layout.hpp"

namespace {

using namespace sst::overlay;

class FakeRenderer final : public IOverlayRenderer {
   public:
    auto Render(const RenderScene& /*scene*/, std::uint32_t w, std::uint32_t h)
        -> RgbaImage override {
        ++renders;
        RgbaImage img;
        img.width = w;
        img.height = h;
        img.stride = w * 4;
        img.pixels.assign(static_cast<std::size_t>(img.stride) * h, 0);
        return img;
    }
    int renders{0};
};

class FakeSink final : public IOverlaySink {
   public:
    auto PushFrame(const RgbaImage& /*frame*/) -> void override { ++pushes; }
    int pushes{0};
};

auto ScoreLayout() -> OverlayLayout {
    OverlayLayout layout;
    layout.canvas_width = 100;
    layout.canvas_height = 100;
    OverlayElement vs;
    vs.id = "vs";
    vs.shape = OverlayShape::kText;
    vs.binding = OverlayBinding::kScoreVs;
    vs.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 100, .y2 = 40, .z = 1};
    vs.visible = true;
    layout.elements.push_back(vs);
    return layout;
}

// A change pushes; an identical refresh does not (push-on-change).
TEST(OverlayControllerTest, PushesOnlyOnChange) {
    FakeRenderer renderer;
    FakeSink sink;
    OverlayController controller(renderer, sink, 100, 100);
    controller.SetLayout(ScoreLayout());

    BindingData data;
    data.score_a = 1;
    data.score_b = 0;
    controller.SetBindingData(data);
    EXPECT_TRUE(controller.Refresh(0));  // first frame -> push
    EXPECT_EQ(sink.pushes, 1);

    // Same data, repeated refreshes: no new push (no thrash).
    controller.SetBindingData(data);
    EXPECT_FALSE(controller.Refresh(0));
    EXPECT_FALSE(controller.Refresh(0));
    EXPECT_EQ(sink.pushes, 1);

    // A real change pushes once more.
    data.score_a = 2;
    controller.SetBindingData(data);
    EXPECT_TRUE(controller.Refresh(0));
    EXPECT_EQ(sink.pushes, 2);
    EXPECT_EQ(renderer.renders, 2);  // rendered exactly per push
}

// Between binding changes the compositor keeps the last buffer — the controller
// simply doesn't push, but the scene still reflects the overlay.
TEST(OverlayControllerTest, NoPushWhenNothingChanges) {
    FakeRenderer renderer;
    FakeSink sink;
    OverlayController controller(renderer, sink, 64, 64);
    controller.SetLayout(ScoreLayout());
    controller.SetBindingData(BindingData{});
    EXPECT_TRUE(controller.Refresh(0));
    const int after_first = sink.pushes;
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(controller.Refresh(static_cast<std::uint64_t>(i)));
    }
    EXPECT_EQ(sink.pushes, after_first);
}

}  // namespace
