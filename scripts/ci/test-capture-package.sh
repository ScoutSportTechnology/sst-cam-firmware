#!/usr/bin/env bash
set -euo pipefail

# Run after `just build`: the diagnostic factory dlopens from the binary's own directory.
test -s dist/sst_cam_firmware-aarch64
test -s dist/libsst_argus_hardware.so
# ELF e_machine 183 (little-endian AArch64) at header offset 18.
test "$(od -An -tu2 -j18 -N2 dist/libsst_argus_hardware.so | tr -d '[:space:]')" = 183
