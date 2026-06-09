---
title: "feat: firmware-side logic alignment with proto contract"
type: feat
status: active
date: 2026-06-09
origin: docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md
---

# feat: Firmware-Side Logic Alignment with Proto Contract

## Summary

Fix the firmware behaviors an audit found diverging from the contract: the proto3 default-inversion that drops or hides overlay elements, non-uniform canvas scaling, missing text-height clipping, the absent text background box, the missing `get_match_state`/`thumbnail` handlers, the absent inbound chunk-ack, and the WiFi-before-session ordering — so the camera renders and responds exactly as the amended proto contract specifies.

---

## Problem Frame

An audit found the firmware shares the wire format with the app but diverges at runtime. The sharpest issues are render correctness: because `visible`/`opacity` are proto3 non-optional, the firmware reads unset `visible` as false (dropping the element) and unset `opacity` as 0.0 (fully transparent), so a camera frame can go blank where the app preview shows a full overlay. The renderer also scales the canvas non-uniformly off-aspect and never clips text to bounds. On transport, firmware acks its own outbound chunks but never inbound command chunks, stalling large app→camera messages. It also enforces a WiFi-Direct-before-session ordering the contract never stated. This plan covers `sst-cam-firmware`; it implements against the amended contract (proto plan `docs/plans/2026-06-09-001-feat-logic-alignment-contract-plan.md`).

---

## Requirements

- R1. Map proto3 defaults safely: unset `visible`→visible, unset `opacity`→1.0 (origin R1).
- R2. Single uniform aspect-preserving scale factor (origin R2).
- R3. Clip text to `bounds` height (origin R3).
- R4. Paint the text `fill_color` background box per the KEEP decision (origin R4).
- R5. Align baseline, font fallback, per-element opacity compositing (origin R5).
- R6. Implement `get_match_state` + `thumbnail` handlers (origin R6).
- R7. Emit a `ChunkAck` per inbound command chunk (origin R7).
- R8. Reconcile WiFi-before-session ordering with the amended §11 (origin R8).

**Origin acceptance examples:** none defined in origin.

---

## Scope Boundaries

- No app or proto changes (sibling plans).
- Mutually-absent commands (`set_wifi_config`, `set_streaming_config`, `factory_reset`, `firmware_update`) stay unsupported.
- No live BLE round-trip or side-by-side render-tolerance harness — per-repo unit tests only.
- Renderer performance tuning beyond correctness is out of scope.

### Cross-Repo Scope (Sibling Plans, Same Release)

Concurrent prerequisites in the same coordinated lockstep release — not later-release work.

- Contract amendments: `sst-cam-proto` plan `docs/plans/2026-06-09-001-feat-logic-alignment-contract-plan.md`.
- App symmetric chunking/render fixes: `sst-cam-app` plan `docs/plans/2026-06-09-016-feat-logic-alignment-app-plan.md`.

---

## Context & Research

### Relevant Code and Patterns

- `src/app/overlay/overlay-proto-mapper.cpp` — `MapStyle` opacity at L49 (`.opacity = s.opacity()`), `MapElement` visible at L66 (`.visible = e.visible()`). Root of the default-inversion.
- `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp` — `DrawText` L78; `Render` L159; non-uniform scale setup L174-179 (`sx`/`sy` then `cairo_scale`).
- `src/app/control/services/dispatcher/command-dispatcher.cpp` — `Register` L10, `Dispatch` L26 (UNSUPPORTED fall-through).
- `src/app/session/services/session_manager/session-manager.cpp` — `OnWifiReady` L38, the gate at L69 (`phase < kWifiReady`).
- `src/adapters/control/ble/bluez/chunk-assembler.cpp` — `OfferInbound` L22, `BeginOutbound` L92, `OnAck` L129.
- `src/adapters/control/ble/bluez/bluez-ble-transport.cpp` — `OnRawCommand` L58.
- `src/app/control/services/handlers/` — per-command handlers (device, download, match, overlay, recording, session, streaming, wifi-direct); register wiring in `src/main.cpp:151-173`.
- Generated proto: build-time protoc with `--experimental_allow_proto3_optional` (`CMakeLists.txt:128-201`) → `has_visible()`/`has_opacity()` available after proto U2.

### Institutional Learnings

- No `docs/solutions/` in this repo. Context lives in `docs/modules/overlay.md`, `docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md`, and `docs/plans/2026-06-08-001-refactor-app-source-of-truth-firmware-plan.md`.

---

## Key Technical Decisions

- Fix the default-inversion at the proto-mapper using `has_visible()`/`has_opacity()` (available once proto U2 makes the fields `optional`), applying the documented defaults (true / 1.0). Rationale: contract-level root fix; mapper is the single translation point.
- Keep the WiFi-before-session ordering (now contract-backed by proto U4) — align the firmware's error/messaging to the contract rather than removing the gate. Rationale: user decision; reflects a real network dependency.
- Implement `get_match_state`/`thumbnail` as new handlers registered through the existing dispatcher port. Rationale: matches the data-driven dispatch pattern; response payloads already exist in the contract.

---

## Open Questions

### Resolved During Planning

- Test framework: GoogleTest; tests auto-globbed (`CONFIGURE_DEPENDS tests/*.cpp`), built/run via `cmake --preset test && cmake --build --preset test && ctest --preset test`.
- Render testing: real Cairo ARGB32 surface + per-pixel `At(img,x,y)` assertions (`tests/overlay/cairo_renderer.test.cpp`); text is currently a no-crash smoke check (fonts may be absent in-container).

### Deferred to Implementation

- Exact `GrabLatest()` frame-snapshot seam (on `PipelineOrchestrator` vs the recording service) — decided in U9 against the real code; the port itself is in scope, only its placement is an implementation detail.

---

## Implementation Units

- U1. **Fix proto3 default mapping for `visible`/`opacity`**

**Goal:** Stop dropping/transparentizing elements when the app omits these fields.

**Requirements:** R1

**Dependencies:** proto plan U2 (fields made `optional`) landed + submodule re-bumped

**Files:**
- Modify: `src/app/overlay/overlay-proto-mapper.cpp` (`MapStyle` L49, `MapElement` L66)
- Test: `tests/overlay/overlay_proto_mapper.test.cpp` (new)

**Approach:**
- `.visible = e.has_visible() ? e.visible() : true;` and `.opacity = s.has_opacity() ? s.opacity() : 1.0f;` (or equivalent), so unset uses the documented default.

**Patterns to follow:** existing `MapStyle`/`MapElement` field copies in the same file; test structure from `tests/overlay/overlay_scene.test.cpp`.

**Test scenarios:**
- Happy path: an element with `visible` set false maps to `visible=false`.
- Edge case: an element with `visible` unset maps to `visible=true` (the blocker fix).
- Edge case: a style with `opacity` unset maps to `1.0`; set `0.5` maps to `0.5`.

**Verification:** elements the app sends without `visible`/`opacity` render normally, not dropped/transparent.

---

- U2. **Uniform aspect-preserving canvas scale**

**Goal:** Replace independent `sx`/`sy` with a single uniform scale factor.

**Requirements:** R2

**Dependencies:** proto plan U3 (uniform-scale MUST)

**Files:**
- Modify: `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp` (scale setup L174-179)
- Test: `tests/overlay/cairo_renderer.test.cpp`

**Approach:**
- Compute one factor (e.g. `min(out_w/canvas_w, out_h/canvas_h)`), apply `cairo_scale(cr, s, s)`; center/letterbox as needed so geometry stays aspect-correct.

**Patterns to follow:** existing canvas→output scaling test cases already in `cairo_renderer.test.cpp`.

**Test scenarios:**
- Happy path: output matching canvas aspect renders unchanged.
- Edge case: output aspect ≠ canvas aspect → a `SHAPE_CIRCLE` in square bounds stays circular (assert inscribed-circle pixels), corner radii unskewed.

**Verification:** off-aspect output no longer stretches circles/radii.

---

- U3. **Clip text to bounds height**

**Goal:** Text must not overflow the bottom of `bounds`.

**Requirements:** R3

**Dependencies:** proto plan U3 (text-clip MUST)

**Files:**
- Modify: `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp` (`DrawText` L78)
- Test: `tests/overlay/cairo_renderer.test.cpp`

**Approach:**
- Apply a Cairo clip rectangle to `bounds` (and/or `pango_layout_set_height`) before painting glyphs so overflowing lines are clipped.

**Patterns to follow:** existing rect/clip pixel assertions in the renderer test.

**Test scenarios:**
- Happy path: text fitting in bounds renders fully.
- Edge case: text taller than bounds is clipped — assert no painted pixels below the bounds bottom edge.

**Verification:** overflowing text clips at the box, matching the contract and the app preview.

---

- U4. **Paint text `fill_color` background box**

**Goal:** For `SHAPE_TEXT` with non-empty `fill_color`, paint the bounds background behind glyphs (KEEP decision).

**Requirements:** R4

**Dependencies:** proto plan U3 (text background-box rule stated)

**Files:**
- Modify: `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp` (`DrawText` L78)
- Test: `tests/overlay/cairo_renderer.test.cpp`

**Approach:**
- Before drawing glyphs, if `fill_color` is non-empty, fill the `bounds` rect with it (respecting opacity), then paint `text_color` glyphs over it.

**Patterns to follow:** the existing rect-fill path (`SHAPE_RECT`) for the background fill.

**Test scenarios:**
- Happy path: a text element with `fill_color` set → assert background pixels inside bounds equal the fill color; glyph color distinct.
- Edge case: empty `fill_color` → no background painted (alpha 0 inside bounds where no glyph).

**Verification:** firmware text background matches the app preview (U6 there).

---

- U5. **Align baseline, font fallback, and opacity compositing**

**Goal:** Bring text baseline/fonts and per-element opacity into contract conformance.

**Requirements:** R5

**Dependencies:** proto plan U1/U3

**Files:**
- Modify: `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp` (`DrawText`, opacity application)
- Test: `tests/overlay/cairo_renderer.test.cpp`

**Approach:**
- Ensure first-line baseline sits one ascent below bounds top; map empty/preferred family to a metric-comparable `sans-serif`/`serif`/`monospace`.
- Opacity is currently applied per-draw-call (`cairo_set_source_rgba` at L113/L131/L144). Co-compositing fill + text as a unit at a single opacity requires rendering the element to an intermediate Cairo group (`cairo_push_group` / `cairo_pop_group_to_source` + `cairo_paint_with_alpha`), replacing the per-call alpha for TEXT-with-fill elements — not just reusing the existing per-call alpha.

**Patterns to follow:** existing opacity≈127 pixel assertion in the renderer test (note it demonstrates per-call alpha, not group compositing).

**Test scenarios:**
- Happy path: opacity 0.5 on a fill+text element composites both at ~50% (assert blended pixels).
- Edge case: empty font family falls back to a shipped comparable face without crashing (smoke, given in-container font limits).

**Verification:** opacity/baseline/font behavior matches the contract within tolerance.

---

- U6. **Implement `get_match_state` handler**

**Goal:** Answer `get_match_state` (which the app polls) instead of returning UNSUPPORTED.

**Requirements:** R6

**Dependencies:** None (proto surface already defined; `LiveMatch` snapshot reachable via `session_.Snapshot()`)

**Files:**
- Create: `src/app/control/services/handlers/match-state.handler.{cpp,hpp}` (or extend `match.handler.cpp`)
- Modify: `src/main.cpp` (register the handler, L151-173)
- Test: `tests/control/match_state_handler.test.cpp` (new)

**Approach:**
- Populate `MatchState` response (correlation_id, status OK) from `LiveMatch` (`score_a`, `score_b`, `period`, `clock_seconds`, `segment`) plus `team_a_id`/`team_b_id` from `state.config`. Register via the dispatcher's `HandledCases()` pattern.

**Patterns to follow:** `tests/control/command_dispatcher.test.cpp` (in-file fake handlers), existing `match.handler.cpp` response population; dispatcher `Register` wiring.

**Test scenarios:**
- Happy path: `get_match_state` returns a `CommandResponse` with `match_state` payload, OK status, echoed correlation_id, populated from the live match snapshot.
- Edge case: dispatcher no longer returns UNSUPPORTED for the `get_match_state` payload case (assert via dispatcher test).

**Verification:** `get_match_state` produces a correct response; dispatcher parity gains one handled case.

---

- U9. **Implement `thumbnail` handler + frame-snapshot port**

**Goal:** Answer `thumbnail` with in-memory JPEG bytes.

**Requirements:** R6

**Dependencies:** None on proto; depends on firmware pipeline architecture — and this unit must BUILD the missing port. Per review + CLAUDE.md ("Pipeline orchestration / Overlay / Storage — Not started"; "No skeletons"): `IRecordingService` has no frame getter, `PipelineOrchestrator` only pushes to an `IFrameSink`, and `IThumbnailWriter` writes to disk via `cv::imwrite`, whereas `ThumbnailResponse.jpeg_bytes` needs in-memory bytes. There is no existing port to "wire to."

**Files:**
- Create: a frame-snapshot port (e.g. `IFrameSnapshotSource` with `std::optional<Frame> GrabLatest()`) — most naturally on `PipelineOrchestrator` (it already calls `Capture()`)
- Create: an in-memory JPEG encoder (`cv::imencode`, sibling to the existing `OpenCvThumbnailWriter`)
- Create: `src/app/control/services/handlers/thumbnail.handler.{cpp,hpp}`
- Modify: `src/main.cpp` (register the handler)
- Test: `tests/control/thumbnail_handler.test.cpp` + a test for the snapshot/encode path (new)

**Approach:**
- Add `GrabLatest()` to the orchestrator/recording path; encode the frame to JPEG in memory via `cv::imencode`; populate `ThumbnailResponse.jpeg_bytes`. When no frame is available, return a real `ResponseStatus::ERROR` (not a `kUnimplemented` skeleton — forbidden by CLAUDE.md).

**Patterns to follow:** `OpenCvThumbnailWriter` (encoder), `IFrameSink`/`PipelineOrchestrator` (snapshot seam), dispatcher `Register` wiring.

**Test scenarios:**
- Happy path: `thumbnail` with a frame available returns a `thumbnail` payload with non-empty JPEG bytes.
- Error path: `thumbnail` with no frame available → `ResponseStatus::ERROR`, not a crash or empty-OK.
- Edge case: dispatcher no longer returns UNSUPPORTED for the `thumbnail` payload case.

**Verification:** `thumbnail` produces JPEG bytes from a live frame; the frame-snapshot port exists and is unit-tested; dispatcher parity table has no remaining app-used UNSUPPORTED.

---

- U7. **Emit `ChunkAck` for inbound command chunks**

**Goal:** Acknowledge each inbound (app→camera) chunk so large commands don't stall.

**Requirements:** R7

**Dependencies:** proto plan U5 (symmetric ack wording)

**Files:**
- Modify: `src/adapters/control/ble/bluez/bluez-ble-transport.cpp` (`OnRawCommand` L58 — emit the ack after `OfferInbound` returns)
- Modify: `src/adapters/control/ble/bluez/gatt-application.hpp` usage (`SendNotification` on the response characteristic)
- Test: `tests/control/chunk_assembler.test.cpp`

**Approach:**
- **The firmware is a GATT peripheral — it cannot "write" to the central.** Its only outbound path is `GattApplication::SendNotification`, which updates the response characteristic (already carrying outbound `CommandResponse` chunks via `BeginOutbound`). So the inbound `ChunkAck` is emitted as a **notification on the response characteristic**, multiplexed alongside command responses; the app demuxes it via the `total_chunks==0` convention (mirroring the firmware's own inbound ack disambiguation at `bluez-ble-transport.cpp:77-83`).
- Emit one `ChunkAck{correlation_id, chunk_index}` per inbound chunk from `OnRawCommand` (after `OfferInbound`), keeping `ChunkAssembler` transport-free (so its unit tests stay transport-free). Keep the existing single-chunk fast path.

**Patterns to follow:** existing outbound `OnAck`/`BeginOutbound`/`SendResponse` notification flow; the `total_chunks==0` ack disambiguation at L77-83.

**Test scenarios:**
- Happy path: a 3-chunk inbound command produces 3 acks (assert via a transport-free fake sink) and reassembles correctly.
- Edge case: single-chunk inbound still acks once (or per contract) and reassembles via fast path.
- Edge case: duplicate inbound chunk index is deduped, ack still well-formed.

**Verification:** large inbound commands (overlay layout, session config) complete without stalling.

---

- U8. **Reconcile WiFi-before-session ordering with §11**

**Goal:** Keep the ordering gate but make its behavior/messaging match the now-documented contract.

**Requirements:** R8

**Dependencies:** proto plan U4 (§11 ordering documented)

**Files:**
- Modify: `src/app/session/services/session_manager/session-manager.cpp` (gate L69, `OnWifiReady` L38)
- Test: `tests/session/session_manager.test.cpp`

**Approach:**
- Keep rejecting session-config-before-WiFi-Direct, but align the error to the contract's stated ordering (clear "out of order: WiFi Direct group required first" semantics per §11). Confirm no extra undocumented constraints remain.

**Patterns to follow:** existing phase-machine tests in `session_manager.test.cpp` (`FakeCleanup`, per-test temp dir).

**Test scenarios:**
- Happy path: session config after WiFi Direct (phase ≥ kWifiReady) is accepted.
- Error path: session config before WiFi Direct is rejected with the contract-aligned ordering error.
- Edge case: overlay-before-session still rejected (existing behavior preserved).

**Verification:** firmware ordering matches §11 exactly — no firmware-only rule beyond the documented flow.

---

## System-Wide Impact

- **Interaction graph:** dispatcher routes by payload case (U6 + U9 add `get_match_state` and `thumbnail`); the proto-mapper is the single overlay translation point (U1); session phase machine gates ordering (U8).
- **Error propagation:** U6 replaces UNSUPPORTED with real responses; U8 aligns the ordering error to the contract.
- **State lifecycle risks:** inbound ack (U7) must not double-ack or leak partial reassembly state on disconnect.
- **API surface parity:** firmware decoders must match app encoders (cross-repo, contract-mediated) — verify field-by-field against `bluetooth.proto`.
- **Unchanged invariants:** wire format and dispatch architecture unchanged; `optional` codegen only adds `has_*()` accessors.

---

## Risks & Dependencies

| Risk | Mitigation |
|------|------------|
| `optional` accessors require the proto re-bump before U1 compiles | Sequence: proto plan U2 + re-bump first; build-time protoc regenerates |
| Text/font tests are unreliable in-container (fonts absent) | Keep font-dependent checks as smoke/no-crash; assert geometry/color on pixels, per existing convention |
| No frame-snapshot port exists (pipeline modules "Not started") — `thumbnail` cannot just "wire to" a pipeline | U9 builds the port (`GrabLatest()` + in-memory `cv::imencode`) as scoped work, not a deferral; returns `ResponseStatus::ERROR` when no frame is available (no skeleton) |
| Hardware-bound BLE transport tests fail in-container | Test the assembler core transport-free (existing pattern); D-Bus wiring validated on device |

---

## Dependencies / Prerequisites

- Proto plan `docs/plans/2026-06-09-001-feat-logic-alignment-contract-plan.md` units U1-U5 landed and the submodule re-bumped in this repo (rebuild regenerates C++ proto with `has_*()`).

---

## Sources & References

- **Origin document:** [docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md](docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md)
- Sibling plans: `sst-cam-proto` `docs/plans/2026-06-09-001-feat-logic-alignment-contract-plan.md`, `sst-cam-app` `docs/plans/2026-06-09-016-feat-logic-alignment-app-plan.md`
- Test patterns: `tests/control/command_dispatcher.test.cpp`, `tests/overlay/cairo_renderer.test.cpp`, `tests/session/session_manager.test.cpp`
