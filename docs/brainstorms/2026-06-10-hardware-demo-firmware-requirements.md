---
date: 2026-06-10
topic: hardware-demo-firmware
---

# Hardware Demo — Firmware Requirements (intelligence-deferred)

## Summary

Wire and ship every firmware module the demo needs to run the full pipeline on the Jetson — orchestration, a static cam-0 decision stand-in, storage, raw dual-camera capture, overlay, RTSP preview, and RTMP/HLS platform streaming — with the AI → physics → decision intelligence stages deliberately replaced by a fixed decision. Inherits scope and definition-of-done from `sst-cam-app/docs/brainstorms/2026-06-10-hardware-demo-system-requirements.md`; this doc feeds sequential ce-plan docs in build order.

---

## Problem Frame

The firmware's modules — config, db, capture, buffer, network/control, processing — are built and unit-tested as isolated hexagonal ports, but they have never run as a wired pipeline. Orchestration, storage, streaming, and overlay are all "not started," and the decision stage that normally drives postprocess is part of the intelligence layer that won't exist for three weeks.

Without a running pipeline there is nothing to demo and nothing for the intelligence work to plug into. The demo forces the firmware to become a moving system on real hardware now, with a trivial fixed decision standing in for intelligence, so that capture/GStreamer/storage/streaming integration risk is retired early and the AI work later lands on proven plumbing.

---

## Requirements

**Orchestration**
- R1. Per-camera worker threads wire capture → preprocess → materialize → per-camera `LatestOnlySlot` buffer, one thread per IMX477, with capture never blocking on downstream.
- R2. A shared consumer thread drives the chosen frame through the decision stand-in → postprocess → final buffer.
- R3. The orchestration is start/stoppable and reports basic running state usable by telemetry.

**Decision stand-in**
- R4. A decision port returns a fixed result — **camera 0, full sensor frame, no crop/zoom** (postprocess resizes to output resolution only).
- R5. The stand-in is a concrete implementation of the same port the real AI/physics/decision will implement, so replacement is a single swap with no orchestration change. (No skeleton: it is a real, shipping implementation, not a placeholder.)

**Storage & raw capture**
- R6. A storage sink consumes the final buffer and records the (optionally overlaid) cam-0 frames to a file on device, start/stop controlled over BLE.
- R7. Recordings are served for download over the WiFi Direct data plane (HTTP with range support).
- R8. A raw-capture path taps **both cameras' materialized frames** and writes per-camera raw files, independent of the decision/postprocess path, start/stop controlled over BLE.
- R9. Raw and final recording can be controlled distinctly (one, the other, or both).

**Overlay**
- R10. An overlay module renders the app-authored banner/scoreboard from app/session state, matching the normative `overlay-rendering.md` semantics.
- R11. Storage and streaming composite the overlay onto the final frame when the operator enabled it; either output can be on or off independently.

**Streaming**
- R12. A streaming sink consumes the final buffer and serves RTSP H.264 over WiFi Direct for live preview on `:8554`.
- R13. A streaming sink pushes the (optionally overlaid) final frames to an external RTMP/HLS platform target supplied by the app.

**Tests**
- R14. Every module added gets module-boundary tests that feed real inputs through the public port and assert real outputs (file on disk, frame on a loopback socket), each test spawning its own dependencies.
- R15. Hardware-bound tests (IMX477 capture, NVENC encode, BlueZ on real D-Bus, wpa_supplicant on real radio) are written and committed even though they fail in the cross-compile container; they pass on-device.

---

## Acceptance Examples

- AE1. **Covers R1, R2, R4, R5.** Given both cameras capturing, when orchestration runs, then cam-0 full-frame bundles flow through postprocess into the final buffer and cam-1 bundles age out of their buffer.
- AE2. **Covers R6, R7.** Given orchestration running, when a record command arrives and later a stop, then a playable cam-0 file exists on device and is range-served over HTTP.
- AE3. **Covers R8, R9.** Given raw-capture started, when a clip is recorded, then two per-camera raw files exist, frame-aligned, without affecting the cam-0 final recording.
- AE4. **Covers R10, R11.** Given overlay enabled with a banner + scoreboard, when storage writes and streaming pushes, then both outputs carry the composited overlay; when disabled, neither does.
- AE5. **Covers R12, R13.** Given orchestration running, when preview is requested, then an RTSP client renders cam-0; when a platform target is set and broadcast starts, then the platform receives the live stream.

---

## Success Criteria

- The full pipeline runs on the Jetson end-to-end with the fixed decision, feeding all four output paths (preview, storage, raw, platform stream).
- Replacing the decision stand-in with real intelligence later touches one port implementation, not the orchestration.
- New modules have green module-boundary tests in the container and committed hardware tests that pass on-device.
- ce-plan can sequence the build from these requirements without inventing firmware behavior.

---

## Scope Boundaries

- No AI/tracking model, no physics, no dynamic decision — fixed cam-0 stand-in only.
- No microphone/dual-mic integration.
- No dynamic camera switching or zoom.
- No new overlay authoring (firmware renders state the app authors).
- Emulator and remote on-device test execution are out (separate session); hardware tests are written, not run here.

---

## Key Decisions

- **Static decision is a real port implementation, not a stub:** honors the repo's no-skeletons rule and makes the intelligence drop-in a clean swap.
- **Raw capture taps the materialized per-camera frames:** those frames already exist in the orchestration (materialize releases the GstBuffer at the buffer boundary), so raw recording is a cheap second sink, not a parallel pipeline.
- **Build order follows the system milestones:** orchestration + decision + postprocess + RTSP first (proof-of-life), then storage + download, then raw capture, then overlay, then RTMP.

---

## Dependencies / Assumptions

- Capture, buffer, processing, network/control modules behave on real hardware as their container unit tests modeled.
- Jetson NVENC is available for H.264 encode for both RTSP and RTMP paths.
- New system deps (e.g. `gst-rtsp-server` for RTSP serving, an RTMP element for platform push) are added to the JetPack sysroot via `.devcontainer/sysroot/003_install_extra_pkgs.sh` and linked `REQUIRED`.
- Proto exposes the commands/descriptors for recording, raw capture, overlay state, and platform target (tracked in the proto requirements doc).

---

## Outstanding Questions

### Deferred to Planning

- [Affects R1, R2][Technical] Thread ownership, lifecycle, and shutdown ordering for per-camera + shared consumer threads.
- [Affects R12, R13][Technical] One encode shared by RTSP + RTMP vs. two independent encodes — NVENC session budget and latency trade-off.
- [Affects R8][Technical] Raw file container/format that is hot-path-cheap to write and labelable for YOLO (e.g. raw NV12 segments vs. encoded), and how cam0/cam1 timestamp alignment is recorded.
- [Affects R13][Needs research] Network topology for RTMP push from the Jetson to an external platform while WiFi Direct serves the phone — does the platform stream go over a second interface?
- [Affects R6, R8][Technical] On-device storage layout and capacity headroom for simultaneous final + dual-raw recording.
