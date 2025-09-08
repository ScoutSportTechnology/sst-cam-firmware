# ScoutCamera Firmware

**On-device software for the ScoutCamera — an open-source sports camera with AI-powered tracking (in progress).**

This repository contains the firmware and runtime stack that runs inside the camera hardware.

## Hardware Journey
- **Early prototype**: Raspberry Pi 5 + dual IMX708 wide-angle cameras.  
- **Current build**: NVIDIA Jetson Orin Nano Super + dual IMX477 HQ cameras.  
- **Upcoming**: Testing microphone integration with two MAX4466 modules.  

## Current Status
- 🎥 **Video streaming** → midway working.  
- 📸 **Image capture** → ~70% complete (needs a few tweaks).  
- 🤖 **AI tracking** → not yet implemented (just moved to Jetson).  
- 🔍 **Zoom algorithm** → in progress (based on ball position, number of players, and other factors).  
- 🎙️ **Microphones** → planned integration (MAX4466).  
- 📡 **Control** → no Bluetooth yet (waiting to test with mobile app features).  
- 💾 **Video storage** → missing (local/archive saving not yet implemented).  
- 🏷️ **Video overlay** → planned, starting with a simple scoreboard.  

## Development Notes
This is a **personal project** I’ve been developing alone and slowly.  
- The code and architecture may not be the most polished — but I’m always open to suggestions and improvements.  
- Expect work-in-progress quality: features evolve as I learn, test, and adapt.  

## Hardware & 3D Parts
I’ve designed 3D-printed parts to mount and assemble the camera. If you’d like to try building one, just reach out and I can share the files.  

## Contributing
- Feedback and ideas are welcome.  
- Code reviews, refactoring suggestions, and performance tips are appreciated.  
- If you’re experimenting with similar hardware, I’d love to hear your results.  

---

⚽ The dream: a fully open-source sports camera that can stream, track, capture, and **broadcast with overlays** — so every field can have its own professional-style coverage.
