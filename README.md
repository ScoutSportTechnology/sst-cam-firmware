# ScoutCamera Firmware

**On-device firmware for ScoutCamera — an open-source AI-assisted sports camera (work in progress).**

This repository contains the firmware and runtime stack that runs directly on the camera hardware.

---

## ⚠️ Development Requirements

This project currently targets only one platform:

- NVIDIA Jetson Orin Nano
- JetPack 6.2.2
- L4T 36.5

Development is done exclusively via the **Dev Container** in this repo, which cross-compiles for aarch64 from an x86_64 host. Native on-device builds are not supported.

1. Install the **Dev Containers** VSCode extension (`ms-vscode-remote.remote-containers`).
2. Open the repo and run **Dev Containers: Reopen in Container**.
3. Once inside the container, configure and build with the `debug` preset (see CLAUDE.md for the full preset list).

Raspberry Pi 5 support has been dropped. All development efforts are focused entirely on Jetson.

---

## Hardware Platform

Current hardware configuration:

- NVIDIA Jetson Orin Nano
- Dual IMX477 HQ cameras
- Planned microphone integration with dual MAX4466 modules

The Raspberry Pi 5 + dual IMX708 prototype phase is complete and no longer supported.

---

## Language & Architecture

The project originally started in Python for rapid prototyping and hardware bring-up.

It has been fully rewritten in modern C++ for:

- Deterministic performance
- Lower latency
- Explicit memory control
- Long-term maintainability on embedded systems

The Python implementation is deprecated and no longer maintained.

---

## Pipeline

End-to-end intended flow:

```
       per-camera (×2)                                       shared            chosen frame only        parallel        outputs
─────────────────────────────────────────────────────  ┌───────────────┐   ┌──────────────────┐   ┌─────────────┐   ┌──────────┐
                                                       │               │   │                  │   │             │   │ storage  │
cam 0 ─► capture ─► preprocess ─► materialize ─► buf ─►│  AI/tracking  │──►│                  │   │             │ ┌►│          │
                                                       │   (per cam)   │   │                  │   │             │ │ └──────────┘
                                                       │       │       │   │  postprocess     │   │             │ │
                                                       │       ▼       │   │  (crop + zoom    │   │  overlay    │ │
                                                       │   physics ────┼──►│   + resize)      │──►│  (banner /  │─┤
                                                       │   (shared)    │   │                  │   │  scoreboard)│ │
                                                       │       │       │   │                  │   │             │ │ ┌──────────┐
                                                       │       ▼       │   │                  │   │             │ └►│          │
cam 1 ─► capture ─► preprocess ─► materialize ─► buf ─►│   decision    │──►│  ─► final buffer │   │             │   │ streaming│
                                                       │ (which cam +  │   │                  │   │             │   │          │
                                                       │  crop rect)   │   │                  │   │             │   └──────────┘
                                                       └───────────────┘   └──────────────────┘   └─────────────┘
```

- **Capture** runs one GStreamer pipeline per IMX477. The appsink is capped at 5 in-flight buffers.
- **Preprocess** turns raw NV12 into a `FrameBundle{ source_frame, ai_frame }`. Grayscale / Binary AI modes touch only the Y plane — no color conversion on the hot path.
- **Materialize** deep-copies the source out of the GstBuffer right before the bundle enters the per-camera buffer, so nothing downstream pins the appsink.
- **AI / tracking** runs per camera and produces detections (field, ball, eventually players).
- **Physics** consumes both cameras' detections and computes trajectory / world-coordinate state.
- **Decision** picks which camera's frame + a crop / zoom rect, then hands them to postprocess.
- **Postprocess** runs once, on the chosen frame only — NV12→BGR, crop, resize, format-convert. Pushes into the final buffer.
- **Overlay** runs in parallel (banner, scoreboard, event info from user/app state).
- **Storage** and **streaming** each consume the final buffer independently and composite the overlay on top if the user enabled it. Either can be on or off.

---

## Module status

Built and working:

- [x] **Config** — JSON device / calibration / storage configs
- [x] **Database** — SQLite + per-entity repositories + seeding
- [x] **Capture** — GStreamer adapter for dual IMX477
- [x] **Buffer** — `LatestOnlySlot`, `DropOldestRing`, plus a `MaterializeFrame` helper so producers can release the GstBuffer the moment a frame enters a downstream buffer
- [x] **Network / control** — WiFi and Bluetooth BLE modules
- [x] **Processing** — `IPreprocessor` / `IPostprocessor` ports + OpenCV adapter (Grayscale / Binary / RGB AI modes; crop + resize + format-convert post)

Not started:

- [ ] **Pipeline orchestration** — per-camera worker threads that wire capture → preprocess → materialize → buffer, and the shared threads for AI → physics → decision → postprocess
- [ ] **AI / tracking** — TensorRT model + adapter; field and ball first, players + jersey numbers later. One inference per camera
- [ ] **Physics** — ball trajectory / world-coordinate projection from both cameras' detections
- [ ] **Decision** — picks which camera's frame + crop / zoom rect; hands off to postprocess
- [ ] **Overlay** — banner / scoreboard / event-info overlays, runs in parallel; storage + streaming composite it on top when enabled
- [ ] **Storage** — local recording / archive of (optionally overlaid) final frames
- [ ] **Streaming** — network output (RTSP / HLS / etc.) of (optionally overlaid) final frames
- [ ] **Microphone** — MAX4466 dual-mic integration

---

## Development Notes

This is a personal project developed solo and iteratively.

- The architecture evolves as hardware, performance constraints, and field testing shape the design
- Expect work-in-progress quality and breaking changes
- Feedback, critiques, and performance insights are welcome

---

## Hardware & 3D Parts

Custom 3D-printed parts are used for mounting and assembly.

If you are building your own unit, the STL files can be shared upon request.

---

## Contributing

- Feedback and ideas are welcome
- Code reviews and performance suggestions are appreciated
- If you are experimenting with similar Jetson hardware setups, sharing results is encouraged

---

⚽ **Goal:** Build a fully open-source sports camera capable of streaming, tracking, capturing, and broadcasting with overlays — bringing professional-style coverage to any field.
