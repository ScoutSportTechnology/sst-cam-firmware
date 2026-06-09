// Cairo/Pango RGBA renderer (U8, R16/R17 + style fidelity).
// Runs in-container (CPU image surface, no GPU/X). Shape + alpha assertions use
// no fonts; a text element is rendered only as a no-crash smoke check.

#include "adapters/overlay/cairo/cairo-overlay-renderer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include "domain/overlay/models/overlay-enums.hpp"
#include "domain/overlay/models/overlay-layout.hpp"
#include "domain/overlay/models/render-scene.hpp"

namespace {

using namespace sst::overlay;
using sst::adapters::overlay::CairoOverlayRenderer;

struct Px {
    std::uint8_t r, g, b, a;
};

auto At(const RgbaImage& img, std::uint32_t x, std::uint32_t y) -> Px {
    const std::uint8_t* p = img.pixels.data() + static_cast<std::size_t>(y) * img.stride + x * 4;
    return Px{p[0], p[1], p[2], p[3]};
}

auto RectElement(float x1, float y1, float x2, float y2, const std::string& fill, float opacity,
                 float radius = 0.0F) -> RenderElement {
    RenderElement e;
    e.shape = OverlayShape::kRect;
    e.bounds = OverlayRect{.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2, .z = 1};
    e.style.fill_color = fill;
    e.style.opacity = opacity;
    e.style.corner_radius = radius;
    return e;
}

// R16/R17: a filled rect renders the fill color inside and transparent outside.
TEST(CairoRendererTest, RectFillAndTransparency) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 100;
    scene.canvas_height = 100;
    scene.elements.push_back(RectElement(10, 10, 60, 60, "#FF0000", 1.0F));

    auto img = renderer.Render(scene, 100, 100);
    ASSERT_EQ(img.width, 100U);
    ASSERT_EQ(img.height, 100U);
    ASSERT_EQ(img.pixels.size(), 100U * 100U * 4U);

    const Px inside = At(img, 30, 30);
    EXPECT_GT(inside.r, 200);
    EXPECT_LT(inside.g, 50);
    EXPECT_LT(inside.b, 50);
    EXPECT_EQ(inside.a, 255);

    const Px outside = At(img, 90, 90);
    EXPECT_EQ(outside.a, 0);  // transparent where nothing was drawn
}

// Output scales canvas -> output resolution (a 2x render keeps the fill).
TEST(CairoRendererTest, ScalesCanvasToOutput) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 100;
    scene.canvas_height = 100;
    scene.elements.push_back(RectElement(0, 0, 100, 100, "#00FF00", 1.0F));

    auto img = renderer.Render(scene, 200, 200);
    ASSERT_EQ(img.width, 200U);
    const Px c = At(img, 100, 100);
    EXPECT_GT(c.g, 200);
    EXPECT_EQ(c.a, 255);
}

// Opacity is honored (half-opacity fill -> alpha near 127).
TEST(CairoRendererTest, OpacityHonored) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 50;
    scene.canvas_height = 50;
    scene.elements.push_back(RectElement(0, 0, 50, 50, "#FFFFFF", 0.5F));

    auto img = renderer.Render(scene, 50, 50);
    const Px c = At(img, 25, 25);
    EXPECT_NEAR(c.a, 127, 12);
}

// Corner radius rounds the rect: the extreme corner pixel is transparent while
// the center is filled.
TEST(CairoRendererTest, CornerRadiusRoundsCorners) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 100;
    scene.canvas_height = 100;
    scene.elements.push_back(RectElement(0, 0, 100, 100, "#FFFFFF", 1.0F, /*radius=*/30.0F));

    auto img = renderer.Render(scene, 100, 100);
    EXPECT_EQ(At(img, 0, 0).a, 0);       // corner cut away
    EXPECT_EQ(At(img, 50, 50).a, 255);   // center filled
}

// Circle fills its center but leaves the bounding-box corners transparent.
TEST(CairoRendererTest, CircleInscribedInBounds) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 100;
    scene.canvas_height = 100;
    RenderElement e;
    e.shape = OverlayShape::kCircle;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 100, .y2 = 100, .z = 1};
    e.style.fill_color = "#0000FF";
    e.style.opacity = 1.0F;
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 100, 100);
    EXPECT_EQ(At(img, 50, 50).a, 255);  // center inside circle
    EXPECT_EQ(At(img, 2, 2).a, 0);      // bbox corner outside circle
}

// Empty scene -> fully transparent surface.
TEST(CairoRendererTest, EmptySceneIsTransparent) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 20;
    scene.canvas_height = 20;
    auto img = renderer.Render(scene, 20, 20);
    for (std::size_t i = 3; i < img.pixels.size(); i += 4) {
        EXPECT_EQ(img.pixels[i], 0);  // every alpha byte is 0
    }
}

// U2: an off-aspect output (wide) keeps a circle inscribed in square canvas
// bounds circular — a uniform scale + letterbox, never an x/y stretch. A
// non-uniform scale would stretch the circle into a wide ellipse, lighting up
// pixels far left/right of center that the inscribed circle never covers.
TEST(CairoRendererTest, UniformScalePreservesCircleOffAspect) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 100;
    scene.canvas_height = 100;
    RenderElement e;
    e.shape = OverlayShape::kCircle;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 100, .y2 = 100, .z = 1};
    e.style.fill_color = "#00FF00";
    e.style.opacity = 1.0F;
    scene.elements.push_back(e);

    // 200x100 output: scale = min(2, 1) = 1, canvas centered horizontally with
    // 50px letterbox bars on each side. A 100px circle inscribed -> radius 50,
    // center at (100, 50).
    auto img = renderer.Render(scene, 200, 100);
    EXPECT_EQ(At(img, 100, 50).a, 255);  // circle center filled
    // If the canvas had been x-stretched to fill 200px width, these far columns
    // would be inside the ellipse; with uniform scale + centering they are in
    // the letterbox and stay transparent.
    EXPECT_EQ(At(img, 5, 50).a, 0);    // left letterbox bar
    EXPECT_EQ(At(img, 195, 50).a, 0);  // right letterbox bar
    // The circle's own bbox corner (within the centered canvas) is outside it.
    EXPECT_EQ(At(img, 52, 2).a, 0);
}

// U3: text taller than its bounds is clipped — no painted pixels below the
// bounds bottom edge. Multi-line text with a tiny-height box forces overflow.
TEST(CairoRendererTest, TextClippedToBoundsHeight) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 200;
    scene.canvas_height = 200;
    RenderElement e;
    e.shape = OverlayShape::kText;
    // A short box (24px tall) that cannot hold three 40px lines.
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 200, .y2 = 24, .z = 1};
    e.style.text_color = "#FFFFFF";
    e.style.font_size = 40.0F;
    e.style.opacity = 1.0F;
    e.text = "AAAA\nBBBB\nCCCC";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 200, 200);
    // No glyph pixel may appear below the bounds bottom edge (y >= 24).
    for (std::uint32_t y = 30; y < 200; ++y) {
        for (std::uint32_t x = 0; x < 200; ++x) {
            EXPECT_EQ(At(img, x, y).a, 0) << "painted pixel below clip at (" << x << "," << y << ")";
        }
    }
}

// U4: a TEXT element with non-empty fill_color paints the bounds background box
// behind the glyphs — assert background pixels inside bounds carry the fill.
TEST(CairoRendererTest, TextFillColorPaintsBackgroundBox) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 120;
    scene.canvas_height = 60;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 120, .y2 = 60, .z = 1};
    e.style.fill_color = "#FF0000";   // red background box
    e.style.text_color = "#FFFFFF";   // white glyphs
    e.style.font_size = 20.0F;
    e.style.opacity = 1.0F;
    e.text = "Hi";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 120, 60);
    // A bottom corner inside bounds is background, not a glyph -> red, opaque.
    const Px bg = At(img, 110, 55);
    EXPECT_GT(bg.r, 200);
    EXPECT_LT(bg.g, 50);
    EXPECT_LT(bg.b, 50);
    EXPECT_EQ(bg.a, 255);
}

// U4 edge: empty fill_color paints no background — inside-bounds pixels with no
// glyph stay fully transparent.
TEST(CairoRendererTest, TextWithoutFillHasTransparentBackground) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 120;
    scene.canvas_height = 60;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 120, .y2 = 60, .z = 1};
    e.style.fill_color = "";        // no background box
    e.style.text_color = "#FFFFFF";
    e.style.font_size = 20.0F;
    e.style.opacity = 1.0F;
    e.text = "Hi";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 120, 60);
    EXPECT_EQ(At(img, 110, 55).a, 0);  // bottom corner, no glyph, no fill
}

// U5: per-element opacity composites the fill+text unit at the given alpha —
// a 0.5-opacity red background box yields ~50% alpha pixels (group compositing,
// not double-darkening).
TEST(CairoRendererTest, TextElementOpacityCompositesAsUnit) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 120;
    scene.canvas_height = 60;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 120, .y2 = 60, .z = 1};
    e.style.fill_color = "#FF0000";
    e.style.text_color = "#FFFFFF";
    e.style.font_size = 20.0F;
    e.style.opacity = 0.5F;
    e.text = "Hi";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 120, 60);
    const Px bg = At(img, 110, 55);  // background-only pixel
    EXPECT_NEAR(bg.a, 127, 12);
    EXPECT_GT(bg.r, 200);  // straight-alpha red preserved
    EXPECT_LT(bg.g, 50);
}

// U5 edge: empty font family falls back to a shipped comparable face without
// crashing (smoke; in-container font availability is limited).
TEST(CairoRendererTest, EmptyFontFamilyFallsBack) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 200;
    scene.canvas_height = 60;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 200, .y2 = 60, .z = 1};
    e.style.text_color = "#FFFFFF";
    e.style.font_family = "";  // unset -> sans-serif fallback
    e.style.font_size = 24.0F;
    e.style.opacity = 1.0F;
    e.text = "Score";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 200, 60);
    EXPECT_EQ(img.width, 200U);
    EXPECT_EQ(img.height, 60U);
}

// #12: the opaque, no-fill text element takes the no-push_group fast path
// (opacity folded into the glyph source color). Glyphs must still paint at full
// alpha and nothing must leak outside the bounds — behavior identical to the
// grouped path. A large white block-glyph string on a tall box guarantees some
// fully-opaque glyph pixel exists.
TEST(CairoRendererTest, OpaqueNoFillTextPaintsGlyphsAtFullAlpha) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 200;
    scene.canvas_height = 80;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 200, .y2 = 80, .z = 1};
    e.style.fill_color = "";          // no background -> no group
    e.style.text_color = "#FFFFFF";   // opaque white glyphs
    e.style.font_size = 48.0F;
    e.style.opacity = 1.0F;           // fully opaque -> no group
    e.text = "MMMM";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 200, 80);
    std::uint8_t max_alpha = 0;
    for (std::uint32_t y = 0; y < 80; ++y) {
        for (std::uint32_t x = 0; x < 200; ++x) {
            max_alpha = std::max(max_alpha, At(img, x, y).a);
        }
    }
    EXPECT_EQ(max_alpha, 255) << "opaque no-fill text should paint full-alpha glyph pixels";
}

// Text element renders without crashing (font availability aside).
TEST(CairoRendererTest, TextRenderDoesNotCrash) {
    CairoOverlayRenderer renderer;
    RenderScene scene;
    scene.canvas_width = 200;
    scene.canvas_height = 60;
    RenderElement e;
    e.shape = OverlayShape::kText;
    e.bounds = OverlayRect{.x1 = 0, .y1 = 0, .x2 = 200, .y2 = 60, .z = 1};
    e.style.text_color = "#FFFFFF";
    e.style.font_size = 32.0F;
    e.text = "2 - 1";
    scene.elements.push_back(e);

    auto img = renderer.Render(scene, 200, 60);
    EXPECT_EQ(img.width, 200U);
    EXPECT_EQ(img.height, 60U);
}

}  // namespace
