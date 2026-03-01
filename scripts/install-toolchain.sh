#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# install-toolchain.sh
# Standalone toolchain installer for Jetson / Raspberry Pi
# Installs build tools:
#   - cmake
#   - ninja
#   - pkg-config
#   - clang / clang++
#
# Usage:
#   ./install-dependencies.sh
# ============================================================

log() { printf "\n\033[1m%s\033[0m\n" "$*"; }
die() { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }

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

log "Checking build tools..."

MISSING_PKGS=()

need_cmd pkg-config || MISSING_PKGS+=("pkg-config")
need_cmd cmake      || MISSING_PKGS+=("cmake")
need_cmd ninja      || MISSING_PKGS+=("ninja-build")
need_cmd clang      || MISSING_PKGS+=("clang")
need_cmd clang++    || MISSING_PKGS+=("clang")

if [[ "${#MISSING_PKGS[@]}" -gt 0 ]]; then
  log "Missing tools detected."
  apt_install "${MISSING_PKGS[@]}"
else
  log "All tools already installed."
fi

log "Verifying..."

need_cmd pkg-config || die "pkg-config still missing after install."
need_cmd cmake      || die "cmake still missing after install."
need_cmd ninja      || die "ninja still missing after install."
need_cmd clang      || die "clang still missing after install."
need_cmd clang++    || die "clang++ still missing after install."

log "Build tools ready."
exit 0