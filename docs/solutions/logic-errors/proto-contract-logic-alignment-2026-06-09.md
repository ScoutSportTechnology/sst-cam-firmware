---
title: "Firmware Logic Alignment with the Amended Proto Contract"
date: 2026-06-09
category: logic-errors
module: control-plane
problem_type: logic_error
component: service_object
symptoms:
  - "Overlay elements with unset visible/opacity dropped or rendered fully transparent (proto3 defaults swallowed intent)"
  - "A single out-of-range opacity poisoned the cairo context and dropped every later element in the frame"
  - "Overlays rendered stretched/skewed on off-aspect outputs; text overflowed bounds with no fill background"
  - "Inbound command chunks ACKed before acceptance and handlers run under the transport lock (stall/deadlock/head-of-line blocking)"
  - "get_match_state/thumbnail returned empty-OK skeletons; timestamps used a monotonic clock instead of wall-clock epoch ms"
root_cause: logic_error
resolution_type: code_fix
severity: high
tags:
  - proto3-defaults
  - overlay-rendering
  - chunk-ack
  - epoch-timestamps
  - ble-control-plane
  - hexagonal-architecture
related_components:
  - overlay
  - control
  - session
  - processing
---

# Firmware Logic Alignment with the Amended Proto Contract

## Problem
A cross-repo audit found the C++/Jetson firmware honoring the app↔firmware wire contract *syntactically* while diverging in *behavior*: it read proto3 non-`optional` scalars raw, mis-rendered overlays off-aspect, acked BLE command chunks before validating them, ran handlers under the transport lock, used the wrong clock for timestamps, and shipped skeleton `get_match_state`/`thumbnail` handlers. Net effect: the camera produced blank/wrong output where the app preview looked correct. (PR #5, `feat/logic-alignment`, merged 2026-06-09.)

## Symptoms
- Overlay elements the app left unset rendered **invisible or fully transparent** even though the app preview showed them (proto3 `false`/`0.0` defaults read as intent).
- A single bad opacity value silently **dropped every later element** in the frame (poisoned `cairo_t` error state).
- Overlays **rendered stretched/skewed** on off-aspect outputs — circles became ellipses, corner radii skewed.
- Text **overflowed its bounds box**; no background box behind text; translucent text **double-darkened** where fill and glyphs overlapped.
- Large multi-chunk commands could **stall or be wrongly acked**; the thumbnail handler **head-of-line-blocked the BLE control path**; a new central could **inherit a previous session's half-assembled message** (+ null-`gatt_app_` crash on disconnect race).
- `get_match_state`/`thumbnail` returned **empty-OK skeletons**, not real data.
- Match-state/thumbnail timestamps used the **monotonic clock** → app decoded wrong absolute times.

## What Didn't Work
The original code trusted proto3 scalar accessors directly — `.opacity()`, `e.visible()` — assuming an unset field would arrive as its intended value. In proto3 an unset non-`optional` scalar is **indistinguishable from one explicitly set to the zero-value**, so unset `visible` read as `false` (dropped) and unset `opacity` read as `0.0` (transparent). The amended contract added `optional` to these fields; the corrected approach gates on `has_*()`. This raw-read baseline is the failed assumption the PR corrects.

## Solution
Real before/after from `git show` on the PR #5 review commits.

**1. Proto3 default gating + opacity clamp** — `src/app/overlay/overlay-proto-mapper.cpp` (`726c98a`):
```cpp
// before
.opacity = s.opacity(),
.visible = e.visible(),
// after
const float opacity = s.has_opacity() ? s.opacity() : 1.0F;   // unset => opaque
.opacity = std::clamp(opacity, 0.0F, 1.0F),                    // clamp at the one mapper boundary
.visible = e.has_visible() ? e.visible() : true,              // unset => renders
```

**2. Uniform aspect-preserving scale** — `src/adapters/overlay/cairo/cairo-overlay-renderer.cpp`:
```cpp
// before: non-uniform x/y stretch
cairo_scale(cr, sx, sy);
// after: single uniform scale + letterbox/center
const double scale = std::min(sx, sy);
cairo_translate(cr, (out_width - canvas_width*scale)/2.0, (out_height - canvas_height*scale)/2.0);
cairo_scale(cr, scale, scale);
```

**3. Text clip-to-bounds + fill box + grouped opacity (with perf gate)** — same file, plus `28f2ccd`:
```cpp
if (fill.valid && w > 0.0 && h > 0.0) {          // paint background box
    RoundedRectPath(cr, x, y, w, h, corner_radius); cairo_fill(cr);
}
if (have_glyphs && w > 0.0 && h > 0.0) {         // clip glyphs to bounds
    cairo_save(cr); cairo_rectangle(cr, x, y, w, h); cairo_clip(cr);
}
// perf #12: only allocate the ~8MB push_group when it actually buys correctness
constexpr double kEps = 1.0/512.0;
const bool needs_group = (opacity < 1.0 - kEps) || fill.valid;
if (needs_group) cairo_push_group(cr);           // opaque no-fill path folds opacity into set_source_rgba
```

**4. ChunkAck: ack-only-after-accept + handler outside lock** — `bluez-ble-transport.cpp` + `chunk-assembler.cpp` (`3983334`):
```cpp
// before: acked BEFORE validating; handler ran under mtx_
SendInboundAck(corr, index);
auto reassembled = assembler_.OfferInbound(chunk);
sst_cam::CommandResponse response = on_command_(command);   // under lock
// after: OfferInbound returns OfferResult{accepted,payload}; ack only if accepted
const ChunkAssembler::OfferResult result = assembler_.OfferInbound(chunk);
if (result.accepted) SendInboundAck(corr, index);   // index>=total / over-cap / total==0 NOT acked
// extract command under lock, DROP lock, run handler, re-lock to send
sst_cam::CommandResponse response = handler(command);       // unlocked (#14)
std::lock_guard lock(mtx_); SendResponse(response);
```

**5. Named sentinel + app-layer move** (`5725cc0`): magic `total_chunks == 0` → `sst::control::kChunkAckTotalChunks`; pure `BuildInboundAck` moved out of the bluez adapter into `src/app/control/services/chunk-ack/`.

**6. Assembler reset on disconnect + null-gatt guard** (`61e3652`):
```cpp
// Stop(): drop assembler state under lock BEFORE clearing gatt_app_
std::lock_guard lock(mtx_); assembler_.Reset(); gatt_app_.reset();
// retained outbound send closure null-guards a post-Stop OnAck race:
if (gatt_app_) gatt_app_->SendNotification(ToBytes(chunk));
```

**7. Real handlers, no skeletons**: `MatchStateHandler` derives `MatchStatus` from live-match segment/clock, zero-clamps `time_remaining_s`, reports `MATCH_NOT_STARTED` with no session config. `ThumbnailHandler` + a **new `IFrameSnapshotSource` port** (`src/app/pipeline/ports/frame-snapshot-source.hpp`) + `OpenCvJpegEncoder` (`89edae7`): `snapshot_.GrabLatest()` → in-memory `cv::imencode(".jpg", ...)`; returns `ResponseStatus::ERROR` (not empty-OK) when no frame exists.

**8. Wall-clock timestamps** — `src/main.cpp` (`3e3c5e2`): added `NowEpochMs()` (`system_clock` epoch ms), wired into match-state/thumbnail handlers in place of monotonic `NowMs`.

**9. §11 ordering message** — `session-manager.cpp` (`4dfbbfd`): `ApplySessionConfig` rejection states the real contract rule (WiFi-Direct group required before config; rejected mid-`kRecording`).

Result: build clean; **ctest 191/197** (the 6 failures are the expected hardware-bound baseline: IMX477/NVENC/BlueZ/wpa/HTTP/RTMP).

## Why This Works
- **proto3 scalar defaults are indistinguishable from unset** — only `has_*()` (enabled by the `optional` keyword in the amended contract) recovers intent; documented defaults (`visible→true`, `opacity→1.0`) are applied at the single proto→domain mapper.
- **cairo error-state propagation**: an out-of-range alpha into `cairo_paint_with_alpha` permanently poisons the `cairo_t`, dropping all later draws — so the clamp must live at the mapper boundary, before the renderer ever sees it.
- **Non-uniform scale breaks geometric invariants** (circles/radii); uniform scale + letterbox preserves them.
- **Transport-lock reentrancy / head-of-line blocking**: holding `mtx_` across a full-res `imencode` froze the control path; ack-before-validate violated flow-control (rejected chunks must not ack; duplicates must).
- **Session state leakage**: assembler state and retained send closures (capturing `gatt_app_`) outlived a disconnect, so a new central inherited stale state and could hit a null deref.

## Prevention
- Always gate proto3 non-`optional` scalars through `has_*()` at the mapper; never read a bare scalar accessor and assume intent.
- **Clamp/normalize at the single proto→domain boundary**, not in the renderer, so no downstream consumer can poison shared state.
- Test explicitly with **unset fields**: assert `has_*()==false → documented default`, and that an explicit `0.0`/`false` is preserved verbatim.
- **Ack-after-accept**: acknowledgements follow validation; cover `index>=total`, over-cap, `total==0`, and duplicate-retransmit.
- Run command handlers **outside the transport lock**; keep only assembler/gatt access locked.
- **Reset per-connection state on disconnect** and null-guard any retained closure capturing connection objects.
- Implement handlers to return real `ERROR` on missing data, never empty-OK skeletons.
- Use `system_clock` for absolute (wire) timestamps; `steady_clock` only for relative durations.

## Related Issues
- Origin plan: `docs/plans/2026-06-09-001-feat-logic-alignment-firmware-plan.md`
- Requirements/brainstorm: `docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md`
- Consolidation that promoted this work to `main`: `docs/solutions/tooling-decisions/cpp-rewrite-promotion-to-main-2026-06-09.md`
- Sibling-repo counterparts (out of repo): `sst-cam-proto`, `sst-cam-app` logic-alignment PRs (proto PR#4, app PR#11).
