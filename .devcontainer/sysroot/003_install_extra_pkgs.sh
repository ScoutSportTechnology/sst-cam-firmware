#!/usr/bin/env bash
# Install Ubuntu jammy aarch64 packages into the JetPack sysroot that are not
# part of the base L4T tarball:
#   - BlueZ peripheral support via sdbus-c++ (BLE control plane)
#   - gst-rtsp-server (companion-app RTSP stream)
#   - gstreamer1.0-plugins-bad (rtmpsink for platform RTMP/RTMPS push)
#
# Runs BEFORE 002_fix_sysroot.sh so the symlink-fix and .so linker-stub passes
# pick up everything that was just extracted.
#
# Usage: 003_install_extra_pkgs.sh <sysroot_path>
set -euo pipefail

SYSROOT="${1:?Usage: 003_install_extra_pkgs.sh <sysroot_path>}"

PORTS="http://ports.ubuntu.com/ubuntu-ports"

# Pinned to Ubuntu 22.04 (jammy) arm64 — matches JetPack 6.x base.
PKGS=(
  # libsystemd-dev: pulls in the .pc file required by sdbus-c++.pc
  "pool/main/s/systemd/libsystemd-dev_249.11-0ubuntu3_arm64.deb"
  # sdbus-c++: BLE peripheral via BlueZ over D-Bus
  "pool/universe/s/sdbus-cpp/libsdbus-c++1_1.1.0-3_arm64.deb"
  "pool/universe/s/sdbus-cpp/libsdbus-c++-dev_1.1.0-3_arm64.deb"
  # gst-rtsp-server: companion-app RTSP server library
  "pool/universe/g/gst-rtsp-server1.0/libgstrtspserver-1.0-0_1.20.1-1_arm64.deb"
  "pool/universe/g/gst-rtsp-server1.0/libgstrtspserver-1.0-dev_1.20.1-1_arm64.deb"
  # gst-plugins-bad: ships rtmpsink (needed for YouTube/Twitch/Facebook/Instagram/
  # custom RTMP+RTMPS push). flvmux/h264parse/mp4mux/qtdemux/concat/splitmuxsink
  # are in plugins-good which is already part of the base sysroot.
  "pool/universe/g/gst-plugins-bad1.0/gstreamer1.0-plugins-bad_1.20.1-1ubuntu1_arm64.deb"
  "pool/universe/g/gst-plugins-bad1.0/libgstreamer-plugins-bad1.0-0_1.20.1-1ubuntu1_arm64.deb"
)

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cd "$TMP"
for path in "${PKGS[@]}"; do
  url="${PORTS}/${path}"
  echo "[003] downloading $url"
  wget -q "$url"
done

for deb in *.deb; do
  echo "[003] extracting $deb -> $SYSROOT"
  dpkg-deb -x "$deb" "$SYSROOT"
done

echo "[003] Done."
