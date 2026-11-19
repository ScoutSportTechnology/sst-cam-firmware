#!/usr/bin/env bash
set -euo pipefail
root="$(dirname "$(realpath "$0")")"
image=ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:jetpack_7.2-jetson_r39.2-v3
docker run --rm --network none --user 0:0 --mount "type=bind,src=$root,dst=/probe" --workdir /probe "$image" bash -euc '
  sdk=/probe/sdk/usr/src/jetson_multimedia_api
  sysroot=/l4t/targetfs
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" -std=c++17 -O2 -Wall -Wextra -Werror -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -DPROBE_SELF_TEST \
    -I"$sdk/argus/include" -I"$sdk/include" -I"$sysroot/usr/local/cuda-13.2/targets/sbsa-linux/include" \
    probe.cpp -o probe-self-test-aarch64
  qemu-aarch64-static -L "$sysroot" ./probe-self-test-aarch64
  aarch64-linux-gnu-g++-13 --sysroot="$sysroot" -std=c++17 -O2 -Wall -Wextra -Werror -pthread \
    -I"$sdk/argus/include" -I"$sdk/include" -I"$sysroot/usr/local/cuda-13.2/targets/sbsa-linux/include" \
    probe.cpp -o probe-aarch64 \
    -L"$sysroot/usr/lib/aarch64-linux-gnu/tegra" -L"$sysroot/usr/lib/aarch64-linux-gnu" \
    -L"$sysroot/usr/local/cuda-13.2/targets/sbsa-linux/lib" \
    -L"$sysroot/usr/local/cuda-13.2/targets/sbsa-linux/lib/stubs" \
    -Wl,-rpath-link,"$sysroot/usr/lib/aarch64-linux-gnu/tegra" \
    -Wl,-rpath-link,"$sysroot/usr/local/cuda-13.2/targets/sbsa-linux/lib" \
    -lnvargus -lnvbufsurface -lEGL -lcudart -lcuda -lpng -lz
  aarch64-linux-gnu-readelf -h probe-aarch64 | /usr/bin/grep -E "Class:|Machine:"
  aarch64-linux-gnu-readelf -d probe-aarch64 | /usr/bin/grep NEEDED
'
