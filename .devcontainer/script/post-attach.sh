#!/usr/bin/env bash
# Runs every time a client attaches to the running container, as the remote user.
# Print a short quick-start banner so the build commands are one glance away.
set -euo pipefail

cat <<'BANNER'
┌─ SST Cam Firmware — cross-compile (Jetson aarch64) ───────────────
│  cmake --preset debug   &&  cmake --build --preset debug
│  cmake --preset test    &&  ctest --preset test
│  binary → build/<preset>/bin/sst_cam_firmware
│  aliases: cfg-debug · build-debug · cfg-test · run-tests
└───────────────────────────────────────────────────────────────────
BANNER
