# ScoutCamera Firmware

**On-device software for ScoutCamera — an open-source sports camera with AI-assisted tracking (work in progress).**

This repository contains the firmware and runtime stack that runs directly on the camera hardware.

## Hardware Journey
- **Early prototype**: Raspberry Pi 5 + dual IMX708 wide-angle cameras
- **Current build**: NVIDIA Jetson Orin Nano Super + dual IMX477 HQ cameras
- **Upcoming**: Microphone testing with dual MAX4466 modules

## Language & Architecture Rewrite
The project originally started in **Python** for practical reasons: fast iteration, rapid prototyping, and hardware bring-up.

It has since been **fully rewritten in C++** with a new architecture designed for:
- Deterministic performance
- Lower latency
- Tighter memory control
- Long-term maintainability on embedded systems

The Python implementation is now deprecated and no longer represents the direction of the project.

## Current Status
- 🎥 **Video streaming** → midway working
- 📸 **Image capture** → ~70% complete (minor fixes pending)
- 🤖 **AI tracking** → not yet implemented (recently migrated to Jetson)
- 🔍 **Zoom algorithm** → in progress (based on ball position, player count, and scene context)
- 🎙️ **Microphones** → planned integration (MAX4466)
- 📡 **Control** → no Bluetooth yet (pending mobile app experiments)
- 💾 **Video storage** → not implemented (local/archive saving pending)
- 🏷️ **Video overlay** → planned (starting with a basic scoreboard)

## Rewrite Architecture Status (C++)
The ongoing C++ rewrite currently includes:

- **Config** → done
- **Capture** → done
- **Frame buffer (latest-only buffer)** → in progress
- **Frame synchronization / pairing** → in progress
- **Pre-process** → planned
- **Inference / feature extraction** → planned
- **Metrics smoothing / tracking** → planned
- **Cross-camera decision** → planned
- **Post-process / composition** → planned
- **Encode / output** → planned
- **Control / configuration** → planned

## Development Notes
This is a **personal project**, developed solo and iteratively.

- The architecture is evolving as hardware, performance constraints, and real-world testing shape the design
- Expect work-in-progress quality and breaking changes
- Suggestions, critiques, and performance insights are welcome

## Hardware & 3D Parts
Custom 3D-printed parts are used for mounting and assembly.
If you want to build one yourself, reach out and the files can be shared.

## Contributing
- Feedback and ideas are welcome
- Code reviews, refactoring suggestions, and performance tips are appreciated
- If you’re experimenting with similar hardware, sharing results is encouraged

---

⚽ **The goal**: a fully open-source sports camera that can stream, track, capture, and **broadcast with overlays** — bringing professional-style coverage to any field.
