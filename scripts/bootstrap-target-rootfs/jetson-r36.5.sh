#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# scripts/bootstrap-target-rootfs/jetson-r36.5.sh
#
# Target rootfs/sysroot bootstrap for Jetson Linux r36.5.
# Stages: download | extract | binaries | rsync | opencv | symlinks | info
#
# Notes:
# - Host prereqs (qemu, toolchain) belong in:
#     scripts/bootstrap-toolchain/wsl-ubuntu.sh
# - binaries stage requires /usr/bin/qemu-aarch64-static and
#   /proc/sys/fs/binfmt_misc/qemu-aarch64 (re-register after WSL restart).
# - opencv stage generates a correct opencv4.pc from the actual libs already
#   present in the L4T rootfs — no apt version mismatch, no missing modules.
# - symlinks stage fixes Debian alternatives (absolute symlinks the cross-
#   linker can't follow) by resolving them to relative paths through the sysroot.
# ============================================================

log()  { printf "\n\033[1m%s\033[0m\n" "$*"; }
warn() { printf "\n\033[33mWARN:\033[0m %s\n" "$*"; }
die()  { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd()    { command -v "$1" >/dev/null 2>&1; }
repo_root()   { (cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); }
hash_stdin()  { sha256sum | awk '{print $1}'; }
run_as_root() { [[ "${EUID:-$(id -u)}" -eq 0 ]] && "$@" || sudo "$@"; }

ensure_sudo() {
  [[ "${EUID:-$(id -u)}" -eq 0 ]] && return
  need_cmd sudo || die "sudo not found. Install sudo or run as root."
}

# ------------------------------------------------------------
# Config
# ------------------------------------------------------------
REL="r36.5"
REL_VER="r36.5.0"
JETSON_SOC="${JETSON_SOC:-t234}"

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
require_arm_chroot_ready() {
  [[ -x /usr/bin/qemu-aarch64-static ]] || die \
    "Missing /usr/bin/qemu-aarch64-static.\nFix: sudo apt-get install -y qemu-user-static"
  [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] || die \
    "Missing binfmt qemu-aarch64.\nFix: ./scripts/bootstrap-toolchain/wsl-ubuntu.sh --stage binfmt --force"
}

chroot_mount() {
  local s="$1"
  run_as_root mkdir -p "$s"/{dev,proc,sys}
  if [[ ! -e "$s/dev/null" ]]; then
    run_as_root mknod -m 666 "$s/dev/null" c 1 3 || true
    run_as_root chmod 666 "$s/dev/null" || true
  fi
  run_as_root mount --bind /dev  "$s/dev"  2>/dev/null || true
  run_as_root mount --bind /proc "$s/proc" 2>/dev/null || true
  run_as_root mount --bind /sys  "$s/sys"  2>/dev/null || true
}

chroot_umount() {
  local s="$1"
  run_as_root umount "$s/sys"  2>/dev/null || true
  run_as_root umount "$s/proc" 2>/dev/null || true
  run_as_root umount "$s/dev"  2>/dev/null || true
}

# ------------------------------------------------------------
# Stages
# ------------------------------------------------------------
stage_download() {
  local fp
  fp="$(printf "bsp=%s\nrootfs=%s\n" "$URL_BSP" "$URL_ROOTFS" | hash_stdin)"
  if fp_match "download" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[download] Up to date."; return 0
  fi

  for pair in "$URL_BSP|$BSP_TBZ2" "$URL_ROOTFS|$ROOTFS_TBZ2"; do
    local url="${pair%%|*}" out="${pair#*|}"
    run_as_root mkdir -p "$(dirname "$out")"
    if [[ -f "$out" && "${FORCE:-0}" -ne 1 ]]; then
      log "[download] Already have: $(basename "$out")"
    else
      log "[download] Fetching: $(basename "$out")"
      curl -L --fail --retry 5 --retry-delay 2 --continue-at - -o "$out" "$url"
    fi
  done
  fp_write "download" "$fp"
}

stage_extract() {
  local fp
  fp="$(printf "rel=%s\n" "$REL_VER" | hash_stdin)"
  if fp_match "extract" "$fp" && [[ "${FORCE:-0}" -ne 1 ]] && [[ -d "$L4T_TEGRA" && -d "$ROOTFS_DIR" ]]; then
    log "[extract] Up to date."; return 0
  fi

  log "[extract] Extracting BSP..."
  run_as_root rm -rf "$L4T_TEGRA"
  run_as_root tar -xjf "$BSP_TBZ2" -C "$L4T_DIR"

  log "[extract] Extracting sample rootfs..."
  run_as_root mkdir -p "$ROOTFS_DIR"
  run_as_root tar -xjf "$ROOTFS_TBZ2" -C "$ROOTFS_DIR"

  fp_write "extract" "$fp"
}

stage_binaries() {
  local fp
  fp="$(printf "apply_binaries=%s\n" "$REL_VER" | hash_stdin)"
  if fp_match "binaries" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[binaries] Up to date."; return 0
  fi

  [[ -x "$L4T_TEGRA/apply_binaries.sh" ]] || die "[binaries] Missing: $L4T_TEGRA/apply_binaries.sh"
  require_arm_chroot_ready

  log "[binaries] Running NVIDIA apply_binaries.sh..."
  run_as_root mkdir -p "$ROOTFS_DIR"/{tmp,var/tmp}
  run_as_root chmod 1777 "$ROOTFS_DIR/tmp" "$ROOTFS_DIR/var/tmp"
  (cd "$L4T_TEGRA" && run_as_root ./apply_binaries.sh)

  fp_write "binaries" "$fp"
}

stage_rsync() {
  local fp
  fp="$(printf "rel=%s\n" "$REL_VER" | hash_stdin)"
  if fp_match "rsync" "$fp" && [[ "${FORCE:-0}" -ne 1 ]] && [[ -d "$OUT_SYSROOT" ]]; then
    log "[rsync] Up to date."; return 0
  fi

  log "[rsync] Syncing rootfs -> sysroot..."
  run_as_root rm -rf "$OUT_SYSROOT"
  run_as_root mkdir -p "$OUT_SYSROOT"
  run_as_root rsync -a --numeric-ids --delete "${ROOTFS_DIR}/" "${OUT_SYSROOT}/"
  fp_write "rsync" "$fp"
}

stage_opencv() {
  local fp
  fp="$(printf "opencv=native-sysroot-v2;soc=%s\n" "$JETSON_SOC" | hash_stdin)"
  local sysroot="$OUT_SYSROOT"
  local multiarch="$sysroot/usr/lib/aarch64-linux-gnu"
  local pc_out="$multiarch/pkgconfig/opencv4.pc"

  if fp_match "opencv" "$fp" && [[ "${FORCE:-0}" -ne 1 ]] && [[ -f "$pc_out" ]]; then
    log "[opencv] Up to date."; return 0
  fi

  # --- Step 1: Ensure headers are present ---
  # The L4T sample rootfs includes OpenCV headers; only fall back to chroot
  # apt if they're missing (e.g. on a fresh sysroot from a minimal rootfs).
  local inc_dir=""
  if [[ -f "$sysroot/usr/include/opencv4/opencv2/core.hpp" ]]; then
    inc_dir="/usr/include/opencv4"
    log "[opencv] Headers found at /usr/include/opencv4 — skipping apt install."
  elif [[ -f "$sysroot/usr/local/include/opencv4/opencv2/core.hpp" ]]; then
    inc_dir="/usr/local/include/opencv4"
    log "[opencv] Headers found at /usr/local/include/opencv4 — skipping apt install."
  else
    log "[opencv] Headers not found; installing via chroot apt..."
    require_arm_chroot_ready

    # Fix <SOC> placeholders in NVIDIA apt sources if present
    if run_as_root grep -Rqs "<SOC>" "$sysroot/etc/apt" 2>/dev/null; then
      run_as_root find "$sysroot/etc/apt" -name "*.list" \
        -exec sed -i "s#<SOC>#${JETSON_SOC}#g" {} +
    fi

    chroot_mount "$sysroot"
    trap 'chroot_umount "$sysroot"' RETURN

    # Install gpgv if missing (required for apt signature verification)
    if ! run_as_root test -x "$sysroot/usr/bin/gpgv"; then
      run_as_root chroot "$sysroot" /bin/bash -lc \
        "apt-get update && apt-get install -y gpgv"
    fi

    run_as_root chroot "$sysroot" /bin/bash -lc \
      "apt-get update && apt-get install -y --download-only libopencv-dev"

    run_as_root bash -s -- "$sysroot" <<'BASH'
set -euo pipefail; shopt -s nullglob
sysroot="$1"
for deb in "$sysroot"/var/cache/apt/archives/*opencv*dev*.deb; do
  dpkg-deb -x "$deb" "$sysroot"
done
BASH

    if   [[ -f "$sysroot/usr/include/opencv4/opencv2/core.hpp" ]];       then inc_dir="/usr/include/opencv4"
    elif [[ -f "$sysroot/usr/local/include/opencv4/opencv2/core.hpp" ]]; then inc_dir="/usr/local/include/opencv4"
    else warn "[opencv] Headers still not found — defaulting to /usr/include/opencv4"; inc_dir="/usr/include/opencv4"
    fi
  fi

  # --- Step 2: Detect which modules actually exist as .so files ---
  # Only list found modules to avoid -lopencv_gapi / -lopencv_videoio errors
  # when those aren't part of the Jetson OpenCV build.
  local libs="" mod
  for mod in highgui videoio stitching video calib3d objdetect dnn features2d \
              imgcodecs photo ml flann imgproc core aruco bgsegm bioinspired \
              ccalib datasets dnn_objdetect dnn_superres dpm face freetype fuzzy \
              hdf hfs img_hash intensity_transform line_descriptor mcc optflow \
              phase_unwrapping plot quality rapid reg rgbd saliency shape stereo \
              structured_light surface_matching text tracking viz wechat_qrcode \
              xobjdetect xphoto ximgproc; do
    ls "$multiarch/libopencv_${mod}.so"* >/dev/null 2>&1 && libs+=" -lopencv_${mod}"
  done
  [[ -n "$libs" ]] || warn "[opencv] No libopencv_*.so files found — Libs will be empty."

  # --- Step 3: Detect version from .so filename (e.g. .so.4.5.4d -> 4.5.4) ---
  local ver
  ver="$(ls "$multiarch/libopencv_core.so."* 2>/dev/null | head -n1 \
        | sed 's/.*\.so\.//' | sed 's/[a-zA-Z]*$//' || echo "unknown")"

  # --- Step 4: Write a correct opencv4.pc describing the actual sysroot ---
  log "[opencv] Writing opencv4.pc (ver=${ver}, libdir=/usr/lib/aarch64-linux-gnu)..."
  run_as_root mkdir -p "$(dirname "$pc_out")"
  run_as_root tee "$pc_out" >/dev/null <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=/usr/lib/aarch64-linux-gnu
includedir=${inc_dir}

Name: OpenCV
Description: Open Source Computer Vision Library (NVIDIA JetPack)
Version: ${ver}
Libs: -L\${libdir}${libs}
Libs.private: -ldl -lm -lpthread -lrt
Cflags: -I\${includedir}
EOF
  log "[opencv] Written: $pc_out"
  fp_write "opencv" "$fp"
}

stage_symlinks() {
  local fp_opencv
  fp_opencv="$(printf "opencv=native-sysroot-v2;soc=%s\n" "$JETSON_SOC" | hash_stdin)"
  local fp
  fp="$(printf "symlinks-v2=%s;opencv=%s\n" "$REL_VER" "$fp_opencv" | hash_stdin)"
  if fp_match "symlinks" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[symlinks] Up to date."; return 0
  fi

  log "[symlinks] Creating compatibility symlinks..."
  local sysroot="$OUT_SYSROOT"
  local multi="$sysroot/usr/lib/aarch64-linux-gnu"
  local usrlib="$sysroot/usr/lib"
  run_as_root mkdir -p "$usrlib" "$multi"

  # Bootlin triplet compat: aarch64-buildroot-linux-gnu -> aarch64-linux-gnu
  run_as_root ln -sfn aarch64-linux-gnu "$usrlib/aarch64-buildroot-linux-gnu"
  [[ -d "$sysroot/lib/aarch64-linux-gnu" || -L "$sysroot/lib/aarch64-linux-gnu" ]] && \
    run_as_root ln -sfn aarch64-linux-gnu "$sysroot/lib/aarch64-buildroot-linux-gnu"

  # crt*.o flat symlinks so the linker finds startup files
  run_as_root bash -s -- "$multi" "$usrlib" <<'BASH'
set -euo pipefail
src="$1" dst="$2"
for f in crt1.o crti.o crtn.o Scrt1.o gcrt1.o grcrt1.o rcrt1.o; do
  [[ -e "$src/$f" ]] && ln -sfn "aarch64-linux-gnu/$f" "$dst/$f"
done
BASH

  # Fix Debian alternatives: the cross-linker follows absolute symlink targets
  # on the HOST filesystem, not inside the sysroot. Walk each absolute symlink
  # chain through the sysroot and replace with a direct relative symlink.
  #
  # Example (libblas/liblapack):
  #   multiarch/libblas.so.3 -> /etc/alternatives/libblas.so.3-aarch64-linux-gnu
  #                          -> /usr/lib/aarch64-linux-gnu/openblas-pthread/libblas.so.3
  #   After fix: libblas.so.3 -> openblas-pthread/libblas.so.3
  log "[symlinks] Fixing absolute symlinks (Debian alternatives)..."
  run_as_root bash -s -- "$sysroot" "$multi" "$sysroot/lib/aarch64-linux-gnu" <<'BASH'
set -euo pipefail
sysroot="$1"; shift
for dir in "$@"; do
  [[ -d "$dir" ]] || continue
  shopt -s nullglob
  for link in "$dir"/*.so "$dir"/*.so.*; do
    [[ -L "$link" ]] || continue
    target=$(readlink "$link")
    [[ "$target" == /* ]] || continue  # already relative — skip
    # Walk the chain through the sysroot
    current="$sysroot$target"
    for _ in 1 2 3 4 5 6 7 8; do
      [[ -L "$current" ]] || break
      next=$(readlink "$current")
      if [[ "$next" == /* ]]; then
        current="$sysroot$next"
      else
        current="$(dirname "$current")/$next"
        break
      fi
    done
    if [[ ! -e "$current" ]]; then
      echo "WARN: [symlinks] broken chain: $link (stopped at $current)" >&2
      continue
    fi
    ln -sf "$(realpath --relative-to="$dir" "$current")" "$link"
  done
done
BASH

  # Create libX.so linker stubs from versioned libX.so.* in multiarch dir
  run_as_root bash -s -- "$multi" <<'BASH'
set -euo pipefail
d="$1"; shopt -s nullglob
for sover in "$d"/*.so.*; do
  base=$(basename "$sover")
  ln -sf "$base" "$d/${base%%.so.*}.so"
done
BASH

  # Remove stale libX.so symlinks from /usr/lib, then mirror from multiarch
  run_as_root bash -s -- "$usrlib" "$multi" <<'BASH'
set -euo pipefail
dst="$1" src="$2"
shopt -s nullglob
for f in "$dst"/lib*.so; do [[ -L "$f" ]] && rm -f "$f"; done
for f in "$src"/*.so; do
  ln -sf "aarch64-linux-gnu/$(basename "$f")" "$dst/$(basename "$f")"
done
BASH

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
ALL_STAGES=(download extract binaries rsync opencv symlinks info)

list_stages() { printf "%s\n" "${ALL_STAGES[@]}"; }

run_stage() {
  case "$1" in
    download)  stage_download  ;;
    extract)   stage_extract   ;;
    binaries)  stage_binaries  ;;
    rsync)     stage_rsync     ;;
    opencv)    stage_opencv    ;;
    symlinks)  stage_symlinks  ;;
    info)      stage_info      ;;
    *) die "Unknown stage: $1" ;;
  esac
}

main() {
  ensure_sudo
  local stage="all"
  FORCE=0

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --stage)       stage="${2:-}"; shift 2 ;;
      --force)       FORCE=1; shift ;;
      --list-stages) list_stages; exit 0 ;;
      -h|--help)
        cat <<EOF
Usage:
  $0                        Run all stages (incremental)
  $0 --stage <name>         Run one stage
  $0 --stage all --force    Force re-run all stages
  $0 --list-stages

Env:
  JETSON_SOC=t234   SoC for NVIDIA apt source <SOC> placeholder (default: t234)

Stages: ${ALL_STAGES[*]}
EOF
        exit 0 ;;
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