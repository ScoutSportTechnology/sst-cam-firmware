---
date: 2026-06-08
topic: app-firmware-logic-alignment
---

# App ⇄ Firmware Logic Alignment — Firmware Slice

## Summary

Fix the firmware behaviors an audit found diverging from the contract — the proto3 default-inversion that drops or hides overlay elements, non-uniform canvas scaling, missing text clipping, the absent inbound chunk-ack, and an undocumented sequencing constraint — so the camera renders and responds exactly as the shared proto contract specifies.

---

## Problem Frame

An audit (2026-06-08) found the firmware shares the wire format with the app but diverges in runtime behavior. The sharpest issues are render correctness: because `visible` and `opacity` are proto3 non-optional, the firmware reads an unset `visible` as false (silently dropping the element) and an unset `opacity` as 0.0 (rendering it fully transparent), so a camera frame can go blank where the app preview shows a full overlay. The renderer also scales the canvas non-uniformly when output aspect differs from canvas aspect, and never clips text to its bounds. On the transport side, firmware acknowledges its own outbound chunks but never acks inbound command chunks, which can stall large app→camera messages. It also enforces a WiFi-Direct-before-session-config ordering the contract never states.

This slice covers the `sst-cam-firmware` repo. Contract amendments are in the proto slice (`proto/docs/brainstorms/2026-06-08-app-firmware-logic-alignment-requirements.md`); app fixes in `sst-cam-app`.

---

## Requirements

**Overlay render correctness**
- R1. Map proto3 defaults safely: an unset `visible` renders the element (default visible) and an unset `opacity` renders fully opaque (default 1.0), per the amended contract — eliminating the default-inversion that drops/hides elements.
- R2. Use a single uniform aspect-preserving scale factor for the canvas instead of independent x/y scaling, so circles and corner radii stay correct at any output aspect.
- R3. Clip text to its `bounds` height (no overflow past the box).
- R4. Render the text `fill_color` background box (or not) consistently with the resolved contract decision (R2 of the proto slice).
- R5. Align baseline placement, font fallback, and per-element opacity compositing (fill + text together) with the contract so two rasterizers stay within tolerance.

**Command dispatch**
- R6. Provide handlers for `get_match_state` and `thumbnail` if the contract keeps them (R6 of the proto slice) — populating the correct response payload, correlation id, and status.

**BLE framing**
- R7. Emit a `ChunkAck` for each inbound (app→camera) command chunk per the contract's flow-control rule, so large inbound commands do not stall.

**Sequencing**
- R8. Reconcile the WiFi-Direct-before-session-config ordering with the contract: either drop the extra constraint or enforce exactly what the amended §11 design flow states (R7 of the proto slice) — no firmware-only ordering rule.

---

## Success Criteria

- For any `OverlayLayout` the app sends, the camera render matches the app preview within the contract tolerance — the audited default-inversion, scaling, and clipping divergences are gone.
- Large inbound commands (overlay layouts, session config) complete over BLE without stalling, and command sequencing matches the contract with no undocumented firmware constraint.
- Every audited firmware finding has a unit test (handler dispatch or Cairo render) that runs in the firmware dev container, and a reviewer can map each test to its finding.

---

## Scope Boundaries

- No app or proto changes in this slice (sibling docs cover those).
- The mutually-absent commands (`set_wifi_config`, `factory_reset`, `firmware_update`) remain unsupported — consistent with the app, which never sends them.
- No live BLE round-trip integration test or side-by-side render-tolerance harness this pass — per-repo unit tests only.
- Performance tuning of the renderer beyond correctness is out of scope.

---

## Key Decisions

- Contract is the binding arbiter; firmware conforms to the (possibly amended) proto + `overlay-rendering.md`, including dropping its own undocumented sequencing rule unless the contract adopts it. Rationale: single source of truth, set in the proto slice.
- Verification is firmware-side unit tests in the dev container, one per finding. Rationale: chosen done-signal.

---

## Dependencies / Assumptions

- Proto slice contract decisions (R2 / R6 / R7 there) gate R4, R6, and R8 here — resolve them first.
- App slice must implement the symmetric chunking/ack and send `visible`/`opacity` explicitly where it matters, but each repo's tests are independent.

---

## Outstanding Questions

### Deferred to Planning

- [Affects R1][Technical] Whether to fix the default mapping at the proto-mapper layer or make the fields `optional` in the contract — decide with the proto slice during planning.
- [Affects R7][Technical] Exact inbound-ack emission point in the BLE transport — answer during planning from the transport code.
