# ScoutCamera Firmware

**On-device firmware for ScoutCamera — an open-source AI-assisted sports camera (work in progress).**

This repository contains the firmware and runtime stack that runs directly on the camera hardware.

---

## ⚠️ Development Requirements

This project currently targets only one platform:

- NVIDIA Jetson Orin Nano
- JetPack 6.2.2
- L4T 36.5

Development is done directly on the Jetson device.

After flashing JetPack 6.2.2 (L4T 36.5), run:

```bash
scripts/bootstrap-jetson-r36.5-toolchain.sh
```

This installs all required toolchain dependencies and system packages required to build the firmware.

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

## Current Status

- 🎥 Video streaming → partially working
- 📸 Image capture → ~70% complete (minor fixes pending)
- 🤖 AI tracking → not implemented yet (post-migration to Jetson)
- 🔍 Zoom algorithm → in progress (based on ball position, player count, and scene context)
- 🎙️ Microphones → planned integration (MAX4466)
- 📡 Control → no Bluetooth yet (mobile app experiments pending)
- 💾 Video storage → not implemented (local/archive saving pending)
- 🏷️ Video overlay → planned (starting with a basic scoreboard overlay)

---

## C++ Architecture Status

The ongoing C++ rewrite currently includes:

- Config → done
- Capture → done
- Frame buffer (latest-only buffer) → in progress
- Frame synchronization / pairing → in progress
- Pre-process → planned
- Inference / feature extraction → planned
- Metrics smoothing / tracking → planned
- Cross-camera decision → planned
- Post-process / composition → planned
- Encode / output → planned
- Control / configuration → planned

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
