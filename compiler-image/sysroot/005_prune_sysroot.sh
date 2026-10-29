#!/usr/bin/env bash
# Prune the assembled L4T r39.2 sysroot down to what the firmware needs to compile,
# link and run its tests under qemu. Runs last in the `l4t` build stage, so the
# deleted files never reach the final image (multi-stage COPY).
#
# Kept on purpose: headers, libraries (incl. the CUDA/cuDNN/TensorRT/VPI stack),
# pkg-config/cmake files, /etc (alternatives symlinks resolve through it), fonts
# (cairo/pango overlay tests may read them through QEMU_LD_PREFIX), NVIDIA
# multimedia/Tegra libs, /opt/nvidia.
#
# Removed: the BSP's flashing payload (bootloader, kernel, flash tools — only the
# rootfs is a sysroot), firmware blobs, boot files, OTA package, sample sources,
# docs/locales/icons/themes/backgrounds, desktop app data, apt/dpkg caches.
#
# Usage: 005_prune_sysroot.sh <l4t_dir>
set -euo pipefail

L4T="${1:?Usage: 005_prune_sysroot.sh <l4t_dir>}"
BSP="$L4T/Linux_for_Tegra"
RFS="$BSP/rootfs"
[[ -d "$RFS/usr" ]] || { echo "[005] ERROR: $RFS is not a rootfs" >&2; exit 1; }

before=$(du -sxm "$L4T" | cut -f1)

# BSP: everything except the rootfs.
find "$BSP" -mindepth 1 -maxdepth 1 ! -name rootfs -exec rm -rf {} +

rm -rf \
  "$RFS"/boot/* \
  "$RFS"/usr/lib/firmware \
  "$RFS"/opt/ota_package \
  "$RFS"/usr/src \
  "$RFS"/var/cache/* \
  "$RFS"/var/lib/apt/lists/* \
  "$RFS"/usr/share/AAVMF \
  "$RFS"/usr/share/doc \
  "$RFS"/usr/share/man \
  "$RFS"/usr/share/info \
  "$RFS"/usr/share/locale \
  "$RFS"/usr/share/icons \
  "$RFS"/usr/share/ibus \
  "$RFS"/usr/share/libreoffice \
  "$RFS"/usr/share/backgrounds \
  "$RFS"/usr/share/themes \
  "$RFS"/usr/share/vim \
  "$RFS"/usr/share/help \
  "$RFS"/usr/share/sounds \
  "$RFS"/usr/share/python-babel-localedata \
  "$RFS"/usr/share/speech-dispatcher

after=$(du -sxm "$L4T" | cut -f1)
echo "[005] Pruned sysroot: ${before} MB -> ${after} MB"
