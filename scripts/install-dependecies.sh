#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# install-dependencies.sh
# Standalone dependency installer for Jetson / Raspberry Pi
# - Ensures toolchain is installed by calling install-toolchain.sh
# - Checks presence via pkg-config
# - Installs missing packages via apt
# - Does NOT build anything
#
# Usage:
#   ./install-dependencies.sh
#
# Intended use in pipelines:
#   ./install-dependencies.sh
#   cmake --preset debug/jetson
#   cmake --build --preset debug/jetson
# ============================================================

log()  { printf "\n\033[1m%s\033[0m\n" "$*"; }
warn() { printf "\n\033[33mWARN:\033[0m %s\n" "$*"; }
die()  { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }
pkg_has()  { pkg-config --exists "$1" >/dev/null 2>&1; }

require_apt() {
  need_cmd apt-get || die "apt-get not found. This script supports Debian/Ubuntu-based OS only."
}

require_sudo_if_needed() {
  if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    need_cmd sudo || die "sudo not found. Install sudo or run this script as root."
  fi
}

apt_update_once() {
  if [[ -z "${_APT_UPDATED:-}" ]]; then
    log "Updating apt package index..."
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
      apt-get update -y
    else
      sudo apt-get update -y
    fi
    _APT_UPDATED=1
  fi
}

apt_install() {
  local pkgs=("$@")
  [[ "${#pkgs[@]}" -gt 0 ]] || return 0

  require_apt
  require_sudo_if_needed
  apt_update_once

  log "Installing: ${pkgs[*]}"
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    apt-get install -y --no-install-recommends "${pkgs[@]}"
  else
    sudo apt-get install -y --no-install-recommends "${pkgs[@]}"
  fi
}

# ------------------------------------------------------------
# Ensure toolchain is installed (cmake/ninja/clang/pkg-config)
# ------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_SCRIPT="${SCRIPT_DIR}/install-toolchain.sh"

if [[ ! -f "${TOOLCHAIN_SCRIPT}" ]]; then
  die "Missing toolchain installer: ${TOOLCHAIN_SCRIPT}"
fi

log "Ensuring toolchain is installed..."
bash "${TOOLCHAIN_SCRIPT}"

# Make sure pkg-config exists now (toolchain script should have installed it)
need_cmd pkg-config || die "pkg-config is still missing after running install-toolchain.sh"

# ------------------------------------------------------------
# GStreamer dev headers + runtime plugins
# Your CMake expects these pkg-config modules:
#   gstreamer-1.0
#   gstreamer-sdp-1.0
#   gstreamer-app-1.0
#   gstreamer-video-1.0
# ------------------------------------------------------------
GST_REQUIRED_MODULES=(gstreamer-1.0 gstreamer-sdp-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

GST_MISSING=0
for mod in "${GST_REQUIRED_MODULES[@]}"; do
  if ! pkg_has "$mod"; then
    GST_MISSING=1
    break
  fi
done

if [[ "$GST_MISSING" -eq 1 ]]; then
  log "GStreamer missing (required pkg-config modules not found). Installing..."
  apt_install \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav
else
  log "GStreamer OK."
fi

# ------------------------------------------------------------
# OpenCV (pkg-config module: opencv4)
# ------------------------------------------------------------
if ! pkg_has opencv4; then
  log "OpenCV missing (opencv4 pkg-config module not found). Installing..."
  apt_install libopencv-dev
else
  log "OpenCV OK."
fi

# ------------------------------------------------------------
# Final verification (fail fast if still missing)
# ------------------------------------------------------------
log "Verifying required pkg-config modules..."
MISSING_FINAL=()

for mod in "${GST_REQUIRED_MODULES[@]}"; do
  pkg_has "$mod" || MISSING_FINAL+=("$mod")
done
pkg_has opencv4 || MISSING_FINAL+=("opencv4")

if [[ "${#MISSING_FINAL[@]}" -gt 0 ]]; then
  die "Still missing pkg-config modules after install: ${MISSING_FINAL[*]}"
fi

log "All dependencies installed and verified."
exit 0