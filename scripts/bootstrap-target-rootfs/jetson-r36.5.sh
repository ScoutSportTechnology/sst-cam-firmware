#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# scripts/bootstrap-target-rootfs/jetson-r36.5.sh
#
# Target rootfs/sysroot bootstrap for Jetson Linux r36.5.
#
# Stages:
#   download | extract | binaries | rsync | symlinks | info
#
# Notes:
# - No host package installation here. Host prereqs belong in:
#     scripts/bootstrap-toolchain/wsl-ubuntu.sh
# - binaries stage runs NVIDIA's apply_binaries.sh, which requires:
#     * /usr/bin/qemu-aarch64-static
#     * /proc/sys/fs/binfmt_misc/qemu-aarch64
#   On WSL, binfmt registration may need to be re-applied after shutdown.
# ============================================================

log()  { printf "\n\033[1m%s\033[0m\n" "$*"; }
warn() { printf "\n\033[33mWARN:\033[0m %s\n" "$*"; }
die()  { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }

ensure_sudo() {
  if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    need_cmd sudo || die "sudo not found. Install sudo or run as root."
  fi
}

run_as_root() {
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then "$@"; else sudo "$@"; fi
}

repo_root() { (cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); }
hash_stdin() { sha256sum | awk '{print $1}'; }

# ------------------------------------------------------------
# Config
# ------------------------------------------------------------
REL="r36.5"
REL_VER="r36.5.0"

URL_BSP="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v5.0/release/Jetson_Linux_r36.5.0_aarch64.tbz2"
URL_ROOTFS="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v5.0/release/Tegra_Linux_Sample-Root-Filesystem_r36.5.0_aarch64.tbz2"

# ------------------------------------------------------------
# Paths
# ------------------------------------------------------------
REPO_ROOT="$(repo_root)"
CACHE_DIR="${REPO_ROOT}/.cache"

DL_DIR="${CACHE_DIR}/downloads/nvidia/${REL}"
L4T_DIR="${CACHE_DIR}/nvidia-l4t/${REL}"
L4T_TEGRA="${L4T_DIR}/Linux_for_Tegra"
ROOTFS_DIR="${L4T_TEGRA}/rootfs"
OUT_SYSROOT="${CACHE_DIR}/sysroot/jetson-${REL}"

BSP_TBZ2="${DL_DIR}/Jetson_Linux_${REL_VER}_aarch64.tbz2"
ROOTFS_TBZ2="${DL_DIR}/Tegra_Linux_Sample-Root-Filesystem_${REL_VER}_aarch64.tbz2"

STAMPS_DIR="${CACHE_DIR}/stamps/bootstrap-rootfs-jetson-${REL}"
mkdir -p "$STAMPS_DIR" "$DL_DIR" "$L4T_DIR"

stamp_file() { echo "${STAMPS_DIR}/$1.stamp"; }
fp_match()   { [[ -f "$(stamp_file "$1")" ]] && grep -qx "$2" "$(stamp_file "$1")"; }
fp_write()   { printf "%s\n" "$2" >"$(stamp_file "$1")"; }

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------
download_file() {
  local url="$1" out="$2"
  run_as_root mkdir -p "$(dirname "$out")"
  if [[ -f "$out" && "${FORCE:-0}" -ne 1 ]]; then
    log "[download] Already have: $(basename "$out")"
    return 0
  fi
  log "[download] Fetching: $(basename "$out")"
  curl -L --fail --retry 5 --retry-delay 2 --continue-at - -o "$out" "$url"
}

tar_extract_tbz2() {
  local tbz2="$1" dest="$2"
  run_as_root mkdir -p "$dest"
  run_as_root tar -xjf "$tbz2" -C "$dest"
}

ensure_rootfs_sane() {
  local rootfs="$1"
  run_as_root mkdir -p "$rootfs/tmp" "$rootfs/var/tmp"
  run_as_root chmod 1777 "$rootfs/tmp" "$rootfs/var/tmp"
}

require_arm_chroot_ready() {
  # apply_binaries.sh will chroot and run dpkg inside the ARM64 rootfs.
  [[ -x /usr/bin/qemu-aarch64-static ]] || die \
"[binaries] Missing /usr/bin/qemu-aarch64-static.
Fix:
  sudo apt-get install -y qemu-user-static"

  [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] || die \
"[binaries] Missing binfmt handler: /proc/sys/fs/binfmt_misc/qemu-aarch64
This is required so the kernel runs ARM64 binaries via QEMU.

Fix (WSL):
  ./scripts/bootstrap-toolchain/wsl-ubuntu.sh --stage binfmt --force"
}

# ------------------------------------------------------------
# Stages
# ------------------------------------------------------------
stage_download() {
  local fp
  fp="$(printf "bsp=%s\nrootfs=%s\n" "$URL_BSP" "$URL_ROOTFS" | hash_stdin)"

  if fp_match "download" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[download] Up to date."
    return 0
  fi

  download_file "$URL_BSP" "$BSP_TBZ2"
  download_file "$URL_ROOTFS" "$ROOTFS_TBZ2"
  fp_write "download" "$fp"
}

stage_extract() {
  local fp
  fp="$(printf "rel=%s\n" "$REL_VER" | hash_stdin)"

  if fp_match "extract" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if [[ -d "$L4T_TEGRA" && -d "$ROOTFS_DIR" ]]; then
      log "[extract] Up to date."
      return 0
    fi
  fi

  log "[extract] Extracting BSP..."
  run_as_root rm -rf "$L4T_TEGRA"
  tar_extract_tbz2 "$BSP_TBZ2" "$L4T_DIR"

  log "[extract] Extracting sample rootfs..."
  run_as_root mkdir -p "$ROOTFS_DIR"
  tar_extract_tbz2 "$ROOTFS_TBZ2" "$ROOTFS_DIR"

  fp_write "extract" "$fp"
}

stage_binaries() {
  local fp
  fp="$(printf "apply_binaries=%s\n" "$REL_VER" | hash_stdin)"

  if fp_match "binaries" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[binaries] Up to date."
    return 0
  fi

  [[ -x "$L4T_TEGRA/apply_binaries.sh" ]] || die "[binaries] Missing: $L4T_TEGRA/apply_binaries.sh"

  require_arm_chroot_ready

  log "[binaries] Running NVIDIA apply_binaries.sh (official)..."
  ensure_rootfs_sane "$ROOTFS_DIR"

  ( cd "$L4T_TEGRA" && run_as_root ./apply_binaries.sh )

  fp_write "binaries" "$fp"
}

stage_rsync() {
  local fp
  fp="$(printf "rel=%s\n" "$REL_VER" | hash_stdin)"

  if fp_match "rsync" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if [[ -d "$OUT_SYSROOT" ]]; then
      log "[rsync] Up to date."
      return 0
    fi
  fi

  log "[rsync] Syncing rootfs -> sysroot cache..."
  run_as_root rm -rf "$OUT_SYSROOT"
  run_as_root mkdir -p "$OUT_SYSROOT"
  run_as_root rsync -a --numeric-ids --delete "${ROOTFS_DIR}/" "${OUT_SYSROOT}/"

  fp_write "rsync" "$fp"
}

stage_symlinks() {
  local fp
  fp="$(printf "symlinks=%s\n" "$REL_VER" | hash_stdin)"

  if fp_match "symlinks" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[symlinks] Up to date."
    return 0
  fi

  log "[symlinks] Creating compatibility symlinks..."

  local sysroot="$OUT_SYSROOT"
  local usr_lib_dir="$sysroot/usr/lib"
  local usr_multi_dir="$sysroot/usr/lib/aarch64-linux-gnu"

  run_as_root mkdir -p "$usr_lib_dir" "$usr_multi_dir"

  # ------------------------------------------------------------
  # Bootlin triplet compatibility
  # ------------------------------------------------------------
  local trip_bootlin="aarch64-buildroot-linux-gnu"
  local trip_ubuntu="aarch64-linux-gnu"

  # /usr/lib/<bootlin-triplet> -> /usr/lib/<ubuntu-triplet>
  run_as_root ln -sfn "$trip_ubuntu" "$sysroot/usr/lib/$trip_bootlin"

  # /lib/<bootlin-triplet> -> /lib/<ubuntu-triplet> (if present)
  if [[ -d "$sysroot/lib/$trip_ubuntu" || -L "$sysroot/lib/$trip_ubuntu" ]]; then
    run_as_root mkdir -p "$sysroot/lib"
    run_as_root ln -sfn "$trip_ubuntu" "$sysroot/lib/$trip_bootlin"
  fi

  # ------------------------------------------------------------
  # Flat startfile symlinks (crt*.o) so ld finds them
  # Some toolchains search $SYSROOT/usr/lib for crt objects.
  # ------------------------------------------------------------
  run_as_root bash -lc "
    set -euo pipefail
    sysroot='$sysroot'
    src='$usr_multi_dir'
    dst='$usr_lib_dir'

    for f in crt1.o crti.o crtn.o Scrt1.o gcrt1.o grcrt1.o rcrt1.o; do
      if [[ -e \"\$src/\$f\" ]]; then
        ln -sfn \"aarch64-linux-gnu/\$f\" \"\$dst/\$f\"
      fi
    done
  "

  # Remove only linker-name symlinks in /usr/lib (safe)
  run_as_root bash -lc "
    set -euo pipefail
    shopt -s nullglob
    d='$usr_lib_dir'
    for f in \"\$d\"/lib*.so; do [[ -L \"\$f\" ]] && rm -f \"\$f\"; done
  "

  # Ensure libX.so exists next to libX.so.* in multiarch dir
  run_as_root bash -lc "
    set -euo pipefail
    shopt -s nullglob
    d='$usr_multi_dir'
    for sover in \"\$d\"/*.so.*; do
      base=\$(basename \"\$sover\")
      name=\${base%%.so.*}.so
      ln -sf \"\$base\" \"\$d/\$name\"
    done
  "

  # Mirror into /usr/lib: libX.so -> aarch64-linux-gnu/libX.so
  run_as_root bash -lc "
    set -euo pipefail
    shopt -s nullglob
    src='$usr_multi_dir'
    dst='$usr_lib_dir'
    for f in \"\$src\"/*.so; do
      name=\$(basename \"\$f\")
      ln -sf \"aarch64-linux-gnu/\$name\" \"\$dst/\$name\"
    done
  "

  fp_write "symlinks" "$fp"
}

stage_info() {
  log "[info] Sysroot ready:"
  printf "  SYSROOT: %s\n" "$OUT_SYSROOT"
  printf "  ROOTFS : %s\n" "$ROOTFS_DIR"
  printf "  BSP    : %s\n" "$L4T_TEGRA"
  printf "  STAMPS : %s\n" "$STAMPS_DIR"
}

# ------------------------------------------------------------
# Stage runner + CLI
# ------------------------------------------------------------
ALL_STAGES=(download extract binaries rsync symlinks info)

list_stages() { printf "%s\n" "${ALL_STAGES[@]}"; }

run_stage() {
  case "$1" in
    download)  stage_download ;;
    extract)   stage_extract ;;
    binaries)  stage_binaries ;;
    rsync)     stage_rsync ;;
    symlinks)  stage_symlinks ;;
    info)      stage_info ;;
    *) die "Unknown stage: $1" ;;
  esac
}

main() {
  ensure_sudo

  local stage="all"
  FORCE=0

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --stage) stage="${2:-}"; shift 2 ;;
      --force) FORCE=1; shift ;;
      --list-stages) list_stages; exit 0 ;;
      -h|--help)
        cat <<EOF
Usage:
  $0                        Run all stages (incremental)
  $0 --stage <name>         Run one stage
  $0 --stage all --force    Force re-run all stages
  $0 --list-stages

Stages: ${ALL_STAGES[*]}
EOF
        exit 0
        ;;
      *) die "Unknown arg: $1" ;;
    esac
  done

  if [[ "$stage" == "all" ]]; then
    for s in "${ALL_STAGES[@]}"; do run_stage "$s"; done
  else
    run_stage "$stage"
  fi
}

main "$@"