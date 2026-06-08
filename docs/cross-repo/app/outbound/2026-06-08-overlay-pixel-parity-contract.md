---
date: 2026-06-08
target_repo: app
source_repo: firmware
status: open
kind: co-development
related: docs/plans/2026-06-08-001-refactor-app-source-of-truth-firmware-plan.md
---

# Overlay pixel-parity: shared fonts + coordinate/anchor/alignment semantics

## Context (firmware side)

The firmware refactor builds a **generic overlay compositor** that renders exactly what `PushOverlayLayout` describes — no hard-coded layouts. The contract's stated goal is **pixel-accurate sync** between the app's in-session Flutter preview and the camera's live stream + recorded MP4: both render the same `OverlayLayout` spec.

The firmware will rasterize the scene with **Cairo (shapes / rounded-rects / alpha) + Pango (text layout, font-family, weight, alignment)** to an RGBA layer, composited identically onto both output branches.

Pixel-parity across two different rendering stacks (Flutter on the app, Cairo/Pango on the camera) is only achievable if both sides agree on rendering semantics and use the **same fonts**.

## What firmware assumes / decided

The firmware interprets `OverlayLayout` / `OverlayElement` / `OverlayRect` / `OverlayStyle` per `proto/bluetooth.proto` as:

- **Coordinate space:** logical canvas pixels (`canvas_width × canvas_height`); `x` right, `y` down; `(0,0)` top-left. The camera maps canvas coords → its output resolution by uniform scale. `OverlayRect{x1,y1,x2,y2}` are the element's edges; `z` is draw order (higher on top).
- **Text:** `font_family` resolved to the **nearest available bundled font**; `font_size` in canvas pixels; `text_align` (left/center/right) within `bounds`; `font_weight` normal/bold; text wraps within `bounds` if needed.
- **Style:** `fill_color`/`text_color` as hex; `opacity` 0–1; `corner_radius` in canvas pixels; `SHAPE_CIRCLE` = ellipse inscribed in `bounds`.
- **Bindings/templates:** clock is `MM:SS`, display-only, ticked locally but driven by `MatchControl`; banner templates substitute `{{param}}` from `BannerEventCommand.params`; command `duration_s` overrides template `duration_ms`.

## Ask for the app repo

1. **Agree on a shared font set.** Which exact font files (family + weights) does the app's Flutter overlay use? The firmware needs to **bundle the identical font files** so glyph shapes/metrics match. Please list them (or propose a canonical set both sides ship).
2. **Confirm the anchor/alignment semantics above** match how the app's Flutter renderer interprets `bounds`, `text_align`, `font_size` (canvas px vs logical pt?), and vertical text positioning within `bounds`. Any divergence (e.g. baseline vs top anchoring, line-height rules) will break parity.
3. **Confirm canvas→surface scaling** is uniform on both sides and that the app's preview uses the same `canvas_width/height` it pushes in the layout.
4. If parity needs a tighter spec than the proto comments provide, let's co-author a short "overlay rendering semantics" appendix to the contract (see the proto-repo handoff lane).

## Acceptance / what "aligned" looks like

- A given `OverlayLayout` renders visually equivalent on the app preview and the camera output (same fonts, same positions, same alignment), within antialiasing tolerance.
- A documented, shared list of bundled fonts and rendering-semantics rules both sides implement.

## Response (fill in from the app session)

<left blank for the app repo's answer/decision>
