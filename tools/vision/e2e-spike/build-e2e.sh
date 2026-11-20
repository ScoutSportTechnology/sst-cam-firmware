#!/usr/bin/env bash
set -euo pipefail
root="$(dirname "$(realpath "$0")")"
image=ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:jetpack_7.2-jetson_r39.2-v4
docker run --rm --network none --user 0:0 --mount "type=bind,src=$root,dst=/spike" --workdir /spike "$image" bash -euc '
  sysroot=/l4t/targetfs
  cuda="$sysroot/usr/local/cuda-13.2/targets/sbsa-linux"
  export PKG_CONFIG_SYSROOT_DIR="$sysroot" PKG_CONFIG_LIBDIR="$sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:$sysroot/usr/lib/pkgconfig:$sysroot/usr/share/pkgconfig" PKG_CONFIG_PATH=
  common=(-std=c++17 -O2 -Wall -Wextra -Werror -pthread -I"$sysroot/usr/include" -I"$sysroot/usr/include/aarch64-linux-gnu" -I"$cuda/include")
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" "${common[@]}" -DE2E_SELF_TEST -ffunction-sections -fdata-sections -Wl,--gc-sections e2e.cpp -o e2e-self-test-aarch64
  qemu-aarch64-static -L "$sysroot" ./e2e-self-test-aarch64
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" "${common[@]}" -DE2E_NVRTC_TEST e2e.cpp -o /tmp/e2e-nvrtc-test-aarch64 -L"$cuda/lib" -lnvrtc
  LD_LIBRARY_PATH="$cuda/lib" qemu-aarch64-static -L "$sysroot" /tmp/e2e-nvrtc-test-aarch64
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" "${common[@]}" e2e.cpp -o e2e-aarch64 \
    $(pkg-config --cflags --libs gstreamer-app-1.0 gstreamer-video-1.0 gstreamer-1.0) \
    -L"$sysroot/usr/lib/aarch64-linux-gnu/tegra" -L"$sysroot/usr/lib/aarch64-linux-gnu" -L"$cuda/lib" -L"$cuda/lib/stubs" \
    -Wl,-rpath-link,"$sysroot/usr/lib/aarch64-linux-gnu/tegra" -Wl,-rpath-link,"$cuda/lib" \
    -lnvargus -lnvbufsurface -lEGL -lcudart -lcuda -lnvrtc -lnvinfer
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" "${common[@]}" engine-build.cpp -o engine-build-aarch64 \
    -L"$sysroot/usr/lib/aarch64-linux-gnu" -lnvonnxparser -lnvinfer
  aarch64-linux-gnu-readelf -h engine-build-aarch64 | /usr/bin/grep -E "Class:|Machine:"
  aarch64-linux-gnu-readelf -d engine-build-aarch64 | /usr/bin/grep NEEDED
  # QEMU can test CLI handling, but cannot build an engine without the Jetson GPU.
  test "$(qemu-aarch64-static -L "$sysroot" ./engine-build-aarch64 >/dev/null 2>&1; echo $?)" = 2
  aarch64-linux-gnu-readelf -h e2e-aarch64 | /usr/bin/grep -E "Class:|Machine:"
  aarch64-linux-gnu-readelf -d e2e-aarch64 | /usr/bin/grep NEEDED
'
