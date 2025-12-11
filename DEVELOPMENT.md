# SST Camera Firmware - Development Guide

## Quick Start

1. Clone the repository:
```bash
git clone https://github.com/ScoutSportTechnology/sst-cam-firmware.git
cd sst-cam-firmware
```

2. Install prerequisites:
   - CMake (≥3.20)
   - Ninja (recommended)
   - C++ compiler (Clang preferred)

3. Build:
```bash
cmake --preset debug
cmake --build --preset debug
```

## Build Commands

- Debug build: `cmake --preset debug && cmake --build --preset debug`
- Release build: `cmake --preset release && cmake --build --preset release`

## Project Structure

```
sst-cam-firmware/
├── CMakeLists.txt          # Build configuration
├── CMakePresets.json       # Build presets
├── include/               # Headers
├── src/                   # Source code
└── build/                 # Build output
```