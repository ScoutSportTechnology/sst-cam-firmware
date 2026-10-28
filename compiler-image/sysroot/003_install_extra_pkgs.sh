#!/usr/bin/env bash
# Install Ubuntu 24.04 (noble) aarch64 packages into the JetPack 7.2 sysroot
# that the L4T r39.2 BSP sample rootfs does not already provide as -dev:
#   - BlueZ peripheral support via sdbus-c++ 1.4.0 (BLE control plane)
#   - gst-rtsp-server 1.24.2 (companion-app RTSP stream)
#   - libprotobuf-dev 3.21.12 headers (runtime .so.32 already in the rootfs;
#     MUST match the host protoc installed in the Dockerfile so generated
#     .pb.cc ABI matches the runtime — noble ships 3.21.12 for both)
#   - OpenCV 4.6 -dev headers (noble stock; runtime .so.406 already present —
#     we deliberately stay on noble 4.6, NOT NVIDIA's 4.8, to match the base
#     sample-rootfs runtime every JP7.2 device ships with; CMakeLists pins .406)
#   - Cairo/Pango/HarfBuzz/etc -dev for the overlay rasterizer
#
# The package URL list lives in noble-deb-urls.txt next to this script. It was
# resolved by apt against the actual BSP rootfs (apt computes the exact dep-
# closed delta + version pins), so it is regenerable rather than hand-curated.
# To regenerate: import the assembled rootfs as an arm64 image and run
#   apt-get install --no-install-recommends --print-uris <pkgs>
# with the NVIDIA jetson repo pinned low (so OpenCV stays noble 4.6).
#
# Runs BEFORE 002_fix_sysroot.sh so the symlink-fix and .so linker-stub passes
# pick up everything that was just extracted.
#
# Usage: 003_install_extra_pkgs.sh <sysroot_path>
set -euo pipefail

SYSROOT="${1:?Usage: 003_install_extra_pkgs.sh <sysroot_path>}"
URLS_FILE="$(dirname "$(readlink -f "$0")")/noble-deb-urls.txt"

if [[ ! -f "$URLS_FILE" ]]; then
  echo "ERROR: package URL list not found: $URLS_FILE" >&2
  exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"

# Download one pinned .deb. The noble -updates/-security pool drops superseded
# revisions, so a pin can start returning 404 (it did for 20 of 69 by 2026-10).
# On failure, fall back to the newest stable-release update of the SAME package in
# the same series: only the last numeric field of the Ubuntu revision may change
# (systemd 255.4-1ubuntu8.16 -> 255.4-1ubuntu8.N, protobuf 3.21.12-8.2ubuntu0.3 ->
# 3.21.12-8.2ubuntu0.N). The pool is shared by all Ubuntu releases, so anything
# else (e.g. protobuf 3.21.12-17 from a later release) is rejected. A no-change
# rebuild revision "buildX.N" may also move to the same series' "ubuntuX.M"
# (bzip2 1.0.8-5.1build0.1 -> 1.0.8-5.1ubuntu0.1). Fails if nothing matches.
WGET=(wget -q --connect-timeout=10 --tries=5)
fetch_deb() {
  local url="$1" dir file pkg rest ver arch base alt cand tail best
  "${WGET[@]}" "$url" && return 0
  dir="${url%/*}"
  file="${url##*/}"; file="${file//%2b/+}"; file="${file//%2B/+}"
  pkg="${file%%_*}"; rest="${file#*_}"; ver="${rest%_*}"; arch="${rest##*_}"; arch="${arch%.deb}"
  base="${ver%.*}."
  alt=""
  if [[ "$base" =~ ^(.*)build([0-9]+)\.$ ]]; then alt="${BASH_REMATCH[1]}ubuntu${BASH_REMATCH[2]}."; fi
  [[ "$ver" == *-*ubuntu*.* || "$ver" == *-*build*.* ]] || {
    echo "[003] ERROR: $file is gone and $ver is not an Ubuntu SRU revision" >&2; return 1; }
  best=""
  while IFS= read -r cand; do
    cand="${cand//%2b/+}"; cand="${cand//%2B/+}"
    for b in "$base" ${alt:+"$alt"}; do
      [[ "$cand" == "${pkg}_${b}"*"_${arch}.deb" ]] || continue
      tail="${cand#"${pkg}_${b}"}"; tail="${tail%"_${arch}.deb"}"
      [[ "$tail" =~ ^[0-9]+$ ]] && best="$cand"
    done
  done < <(curl -fsS --connect-timeout 10 --retry 5 "$dir/" \
             | grep -oE 'href="[^"]+\.deb"' | sed 's/^href="//; s/"$//; s/%2[bB]/+/g' | sort -V)
  if [[ -z "$best" ]]; then
    echo "[003] ERROR: $file is gone and no ${pkg}_${base}N revision exists in $dir" >&2
    return 1
  fi
  echo "[003] WARN: $file is gone from the archive; using $best"
  "${WGET[@]}" "$dir/$best"
}

while IFS= read -r url; do
  [[ -z "$url" || "$url" == \#* ]] && continue
  echo "[003] downloading $url"
  fetch_deb "$url"
done < "$URLS_FILE"

for deb in *.deb; do
  echo "[003] extracting $deb -> $SYSROOT"
  dpkg-deb -x "$deb" "$SYSROOT"
done

echo "[003] Done ($(ls -1 ./*.deb | wc -l) packages)."
