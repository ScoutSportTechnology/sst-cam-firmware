#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# bootstrap-cross-compiling.sh
#
# Intended to run INSIDE:
#   nvcr.io/nvidia/jetpack-linux-aarch64-crosscompile-x86:<SW_VERSION>
#
# Persists extracted sysroot + toolchain into a mounted cache.
# ============================================================

log() { printf "\n\033[1m%s\033[0m\n" "$*"; }
die() { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

# ---- where we persist stuff (you mounted ../.cache -> /.cache) ----
CACHE_ROOT="${CACHE_ROOT:-/.cache}"
L4T_CACHE_DIR="${L4T_CACHE_DIR:-$CACHE_ROOT/nvidia-l4t}"

TARGETFS_DIR="${TARGETFS_DIR:-$L4T_CACHE_DIR/targetfs}"      # sysroot
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$L4T_CACHE_DIR/toolchain}"    # extracted toolchain root

# Expected toolchain subdir in JetPack 6 container
TOOLCHAIN_TRIPLE_DIR_REL="aarch64--glibc--stable-2022.08-1"
TOOLCHAIN_TRIPLE_DIR="$TOOLCHAIN_DIR/$TOOLCHAIN_TRIPLE_DIR_REL"

# ---- sanity checks: must be NVIDIA cross container layout ----
[[ -d /l4t ]] || die "Missing /l4t. Are you running inside NVIDIA jetpack-linux-aarch64-crosscompile-x86 container?"
command -v lbzip2 >/dev/null 2>&1 || die "lbzip2 not found in container. (It should exist in the NVIDIA image.)"

mkdir -p "$L4T_CACHE_DIR"

# ============================================================
# 1) Extract targetfs (sysroot) once
# ============================================================
if [[ ! -d "$TARGETFS_DIR/usr" ]]; then
  log "Extracting targetfs -> $TARGETFS_DIR (first-time setup)"
  mkdir -p "$TARGETFS_DIR"

  # NVIDIA provides split archives under /l4t
  pushd /l4t >/dev/null

  # Rebuild the tarball only if needed
  TMP_TAR="/tmp/targetfs.tbz2"
  if [[ ! -f "$TMP_TAR" ]]; then
    log "Reassembling targetfs.tbz2 from targetfs.tbz2.*"
    cat targetfs.tbz2.* > "$TMP_TAR"
  fi

  log "Unpacking targetfs (this is big)"
  # Extract into TARGETFS_DIR
  tar -I lbzip2 -xf "$TMP_TAR" -C "$TARGETFS_DIR"

  popd >/dev/null
else
  log "targetfs already present: $TARGETFS_DIR"
fi

# ============================================================
# 2) Extract toolchain once
# ============================================================
if [[ ! -d "$TOOLCHAIN_TRIPLE_DIR/bin" ]]; then
  log "Extracting toolchain -> $TOOLCHAIN_DIR (first-time setup)"
  mkdir -p "$TOOLCHAIN_DIR"
  pushd /l4t >/dev/null

  # JetPack 6 uses toolchain.tar.bz2
  [[ -f toolchain.tar.bz2 ]] || die "Missing /l4t/toolchain.tar.bz2"
  tar -xf toolchain.tar.bz2 -C "$TOOLCHAIN_DIR"

  popd >/dev/null
else
  log "toolchain already present: $TOOLCHAIN_TRIPLE_DIR"
fi

# ============================================================
# 3) Export cross-compile environment (JetPack 6 style)
# ============================================================
export TOOLCHAIN_DIR
export CROSS_COMPILE_AARCH64_PATH="$TOOLCHAIN_TRIPLE_DIR"
export CROSS_COMPILE_AARCH64="$CROSS_COMPILE_AARCH64_PATH/bin/aarch64-buildroot-linux-gnu-"

# Convenience aliases (some build systems look for CROSS_COMPILE)
export CROSS_COMPILE="${CROSS_COMPILE_AARCH64}"

# Sysroot
export TARGET_ROOTFS="$TARGETFS_DIR"
export SYSROOT="$TARGETFS_DIR"

# Put toolchain first in PATH
export PATH="$CROSS_COMPILE_AARCH64_PATH/bin:$PATH"

# ============================================================
# 4) pkg-config setup so it resolves Jetson libs from sysroot
# ============================================================
# Many Jetson packages provide .pc files under these locations.
# We force pkg-config to look inside the sysroot and nowhere else.
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

# Common pkgconfig dirs on Jetson rootfs
PKGCFG_DIRS=(
  "$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig"
  "$SYSROOT/usr/lib/pkgconfig"
  "$SYSROOT/usr/share/pkgconfig"
  "$SYSROOT/usr/local/lib/aarch64-linux-gnu/pkgconfig"
  "$SYSROOT/usr/local/lib/pkgconfig"
)

# Build PKG_CONFIG_LIBDIR with only existing dirs
PKG_CONFIG_LIBDIR=""
for d in "${PKGCFG_DIRS[@]}"; do
  if [[ -d "$d" ]]; then
    if [[ -z "$PKG_CONFIG_LIBDIR" ]]; then
      PKG_CONFIG_LIBDIR="$d"
    else
      PKG_CONFIG_LIBDIR="$PKG_CONFIG_LIBDIR:$d"
    fi
  fi
done
export PKG_CONFIG_LIBDIR

# Helpful for some projects
export CMAKE_SYSROOT="$SYSROOT"

# ============================================================
# 5) CUDA / VPI / TensorRT "discovery" variables
# ============================================================
# These are mostly for CMake-based projects. Actual libs/headers must be in the sysroot.
# On Jetson, CUDA is usually in /usr/local/cuda
if [[ -d "$SYSROOT/usr/local/cuda" ]]; then
  export CUDA_HOME="$SYSROOT/usr/local/cuda"
  export CUDA_TOOLKIT_ROOT_DIR="$CUDA_HOME"
fi

# TensorRT headers usually under /usr/include/aarch64-linux-gnu or /usr/include
# libs under /usr/lib/aarch64-linux-gnu
# We'll set common hints; your CMake can use these.
export TENSORRT_ROOT="${TENSORRT_ROOT:-$SYSROOT/usr}"
export TRT_LIB_DIR="${TRT_LIB_DIR:-$SYSROOT/usr/lib/aarch64-linux-gnu}"

# VPI usually under /opt/nvidia/vpi3 on JetPack 6
if [[ -d "$SYSROOT/opt/nvidia/vpi3" ]]; then
  export VPI_ROOT="$SYSROOT/opt/nvidia/vpi3"
fi

# Extra include/library hints for builds that don't use pkg-config well
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}"
# Add a few likely prefixes inside sysroot (don’t duplicate endlessly)
add_prefix() {
  local p="$1"
  [[ -d "$p" ]] || return 0
  case ":$CMAKE_PREFIX_PATH:" in
    *":$p:"*) ;;
    *) CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:+$CMAKE_PREFIX_PATH:}$p" ;;
  esac
}
add_prefix "$SYSROOT/usr"
add_prefix "$SYSROOT/usr/local"
add_prefix "${CUDA_HOME:-}"
add_prefix "${VPI_ROOT:-}"
export CMAKE_PREFIX_PATH

# ============================================================
# 6) Print a short summary (so you can tell it's working)
# ============================================================
log "Cross compile environment ready"
echo "  SYSROOT                 = $SYSROOT"
echo "  TOOLCHAIN_DIR           = $TOOLCHAIN_DIR"
echo "  CROSS_COMPILE_AARCH64    = $CROSS_COMPILE_AARCH64"
echo "  PKG_CONFIG_SYSROOT_DIR   = ${PKG_CONFIG_SYSROOT_DIR:-}"
echo "  PKG_CONFIG_LIBDIR        = ${PKG_CONFIG_LIBDIR:-}"
echo "  CUDA_HOME                = ${CUDA_HOME:-<not found in sysroot>}"
echo "  VPI_ROOT                 = ${VPI_ROOT:-<not found in sysroot>}"
echo "  TENSORRT_ROOT            = ${TENSORRT_ROOT:-}"
echo "  TRT_LIB_DIR              = ${TRT_LIB_DIR:-}"

# Optional: quick compiler sanity (doesn't build anything)
if command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
  echo "  aarch64 gcc              = $("${CROSS_COMPILE}gcc" --version | head -n1)"
fi