#!/usr/bin/env bash
# Install the JetPack 7.2 AI stack (CUDA 13.2, cuDNN 9.20, TensorRT 10.16, VPI 4.1)
# into the L4T r39.2 sysroot, so the upcoming inference pipelines can be built
# without changing the image. The firmware does not use them yet.
#
# Packages come from NVIDIA's Jetson apt repo (r39.2), the same source the device
# uses. Versions are pinned to the JetPack 7.2 GA set (nvidia-jetpack 7.2-b187),
# matching the R39.2.0 BSP; bump all of them together when moving to 7.2.x.
# apt runs inside the arm64 rootfs under qemu (like apply_binaries.sh), so
# dependencies, alternatives (/usr/local/cuda) and ldconfig are handled by dpkg.
#
# Removed after install: static archives and TensorRT builder resources (below).
# Not installed: nvcc (the repo's nvcc is an aarch64 binary; a host-side CUDA
# cross compiler is only needed for custom .cu kernels / TensorRT plugins),
# samples, Python bindings, docs.
#
# Usage: 004_install_ai_stack.sh <sysroot_path>
set -euo pipefail

SYSROOT="${1:?Usage: 004_install_ai_stack.sh <sysroot_path>}"

CUDA_VER="13.2.75-1"
CUDA_LIBS_VER="13.2.1-1"
CUDNN_VER="9.20.0.46-1"
TRT_VER="10.16.2.10-1+cuda13.2"
VPI_VER="4.1.3"
CRT_VER="13.2.78-1"      # cuda-crt of the 13.2.1 (GA) toolkit

PKGS=(
  "cuda-cudart-dev-13-2=${CUDA_VER}"
  "cuda-driver-dev-13-2=${CUDA_VER}"
  "cuda-libraries-dev-13-2=${CUDA_LIBS_VER}"
  # /usr/local/cuda and /usr/local/cuda-13 alternatives, as on the device
  "cuda-toolkit-config-common=${CUDA_VER}"
  "cuda-toolkit-13-config-common=${CUDA_VER}"
  "cuda-toolkit-13-2-config-common=${CUDA_VER}"
  "libcudnn9-cuda-13=${CUDNN_VER}"
  "libcudnn9-dev-cuda-13=${CUDNN_VER}"
  "libnvinfer10=${TRT_VER}"
  "libnvinfer-dev=${TRT_VER}"
  "libnvinfer-headers-dev=${TRT_VER}"
  "libnvinfer-plugin10=${TRT_VER}"
  "libnvinfer-plugin-dev=${TRT_VER}"
  "libnvinfer-headers-plugin-dev=${TRT_VER}"
  "libnvonnxparsers10=${TRT_VER}"
  "libnvonnxparsers-dev=${TRT_VER}"
  "libnvvpi4=${VPI_VER}"
  "vpi4-dev=${VPI_VER}"
)

# Name resolution inside the chroot (the sample rootfs ships a dangling
# systemd-resolved symlink).
RESOLV="$SYSROOT/etc/resolv.conf"
mv "$RESOLV" "$RESOLV.l4t" 2>/dev/null || true
cp /etc/resolv.conf "$RESOLV"
# apt/apt-key need real /dev/null etc. inside the chroot; earlier chrooted steps
# can leave a plain *file* at dev/null, which makes signature checks fail with
# "gpgv ... required for verification". Docker build allows mknod, not mount.
DEVS=(null:1:3 zero:1:5 random:1:8 urandom:1:9)
for d in "${DEVS[@]}"; do
  IFS=: read -r n maj min <<<"$d"
  rm -f "$SYSROOT/dev/$n"; mknod -m 666 "$SYSROOT/dev/$n" c "$maj" "$min"
done
# Desktop apt hooks (update-notifier, packagekit, appstream, icons, esm, snapd)
# run Python/dbus helpers after every update; under qemu they take many minutes
# and add nothing to a sysroot. Park them for this step.
HOOKS_DIR="$SYSROOT/etc/apt/apt.conf.d"
PARKED=$(mktemp -d)
for h in 10periodic 15update-stamp 20apt-esm-hook.conf 20dbus 20packagekit 20snapd.conf \
         50appstream 60icons 60icons-hidpi 99update-notifier; do
  [[ -e "$HOOKS_DIR/$h" ]] && mv "$HOOKS_DIR/$h" "$PARKED/"
done
restore() {
  rm -f "$RESOLV"; mv "$RESOLV.l4t" "$RESOLV" 2>/dev/null || true
  for d in "${DEVS[@]}"; do rm -f "$SYSROOT/dev/${d%%:*}"; done
  mv "$PARKED"/* "$HOOKS_DIR/" 2>/dev/null || true; rmdir "$PARKED" 2>/dev/null || true
}
trap restore EXIT

export DEBIAN_FRONTEND=noninteractive
CH=(chroot "$SYSROOT")
echo "[004] apt-get update (NVIDIA r39.2 + Ubuntu noble) ..."
"${CH[@]}" apt-get update -o Acquire::Retries=5 -o Acquire::http::Timeout=20
# The CUDA library metas only declare ">=" floors, so apt would mix in newer
# 7.2.x point-release libraries (e.g. cuBLAS 13.4.1 from 7.2.1). Turn the floors of
# the pinned metas into exact pins so the whole set is the JetPack 7.2 GA set.
floor_pins() {
  "${CH[@]}" apt-cache show "$1" | sed -n '0,/^Depends: /s/^Depends: //p' | tr ',' '\n' \
    | sed -E 's/^ +//; s/ \(>= ([^)]+)\)$/=\1/; s/ \(= ([^)]+)\)$/=\1/' | grep '='
}
# A floor names the upstream version ("13.2.75"); map it to the repo's full
# Debian version ("13.2.75-1").
exact_pin() {
  local name="${1%%=*}" floor="${1#*=}" v
  v=$("${CH[@]}" apt-cache madison "$name" \
        | awk -v f="$floor" '$3 == f || index($3, f "-") == 1 {print $3}' | sort -V | head -1)
  [[ -n "$v" ]] || { echo "[004] ERROR: no ${name} version matching ${floor}" >&2; return 1; }
  echo "${name}=${v}"
}
mapfile -t FLOOR_RAW < <(floor_pins "cuda-libraries-dev-13-2=${CUDA_LIBS_VER}"; \
                         floor_pins "cuda-libraries-13-2=${CUDA_LIBS_VER}")
FLOORS=()
for f in "${FLOOR_RAW[@]}"; do FLOORS+=("$(exact_pin "$f")"); done
[[ ${#FLOORS[@]} -gt 10 ]] || { echo "[004] ERROR: could not resolve CUDA library pins" >&2; exit 1; }
PKGS+=("cuda-libraries-13-2=${CUDA_LIBS_VER}" "${FLOORS[@]}")

echo "[004] Installing: ${PKGS[*]}"
"${CH[@]}" apt-get install -y --no-install-recommends \
    -o Acquire::Retries=5 -o Acquire::http::Timeout=20 \
    -o Dpkg::Options::=--force-confold "${PKGS[@]}"
# cuda_runtime_api.h includes crt/host_defines.h, which ships in cuda-crt-13-2.
# That package Depends on build-essential (it belongs to nvcc), so unpack just
# its headers instead of pulling a native arm64 compiler into the sysroot.
"${CH[@]}" sh -c "cd /tmp && apt-get download -o Acquire::Retries=5 cuda-crt-13-2=${CRT_VER}"
dpkg-deb -x "$SYSROOT/tmp/cuda-crt-13-2_${CRT_VER}_arm64.deb" "$SYSROOT/"
rm -f "$SYSROOT"/tmp/cuda-crt-13-2_*.deb
"${CH[@]}" apt-get clean
rm -rf "$SYSROOT"/var/lib/apt/lists/*

# Drop what a cross-compile sysroot never uses (~7.5 GB). The firmware links the
# shared libraries and runs against the device's own copies, so:
# - static archives go (libnvinfer_static.a alone is 3.3 GB), except the small
#   CUDA runtime archive CMake's CUDA support links by default;
# - TensorRT builder resources go: the engine builder dlopen()s them at runtime
#   on the GPU (none of the shared libraries lists them as DT_NEEDED).
before=$(du -sm "$SYSROOT/" | cut -f1)
find "$SYSROOT"/usr/local/cuda-13.2/targets/*/lib -maxdepth 1 -name 'lib*_static.a' \
     ! -name 'libcudart_static.a' -delete
find "$SYSROOT"/usr/lib/aarch64-linux-gnu -maxdepth 1 \
     \( -name 'libnvinfer*_static.a' -o -name 'libnvonnxparser*_static.a' \
        -o -name 'libcudnn*_static*.a' -o -name 'libnvinfer_builder_resource*' \) -delete
echo "[004] Dropped static archives + TensorRT builder resources: ${before} MB -> $(du -sm "$SYSROOT/" | cut -f1) MB"

# Fail the build if anything the pipelines will need is missing.
for f in usr/local/cuda/include/cuda_runtime.h \
         usr/local/cuda/include/crt/host_defines.h \
         usr/local/cuda/lib64/libcudart.so \
         usr/include/aarch64-linux-gnu/NvInfer.h \
         usr/include/aarch64-linux-gnu/NvOnnxParser.h \
         usr/lib/aarch64-linux-gnu/libnvinfer.so \
         usr/lib/aarch64-linux-gnu/libnvonnxparser.so \
         usr/include/aarch64-linux-gnu/cudnn.h \
         usr/lib/aarch64-linux-gnu/libcudnn.so \
         opt/nvidia/vpi4/include/vpi/VPI.h; do
    # Checked inside the chroot: many of these are absolute symlinks
    # (/etc/alternatives/...) that only resolve relative to the sysroot.
    "${CH[@]}" test -e "/$f" || { echo "[004] ERROR: missing $f" >&2; exit 1; }
done
echo "[004] AI stack installed (CUDA ${CUDA_VER}, cuDNN ${CUDNN_VER}, TensorRT ${TRT_VER}, VPI ${VPI_VER})."
