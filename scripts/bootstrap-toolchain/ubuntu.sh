#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# scripts/bootstrap-toolchain/ubuntu.sh
#
# Host toolchain bootstrap for native Ubuntu x86_64.
# (Not WSL — use wsl-ubuntu.sh for WSL environments.)
#
# Installs:
#   - base dev tools (incl. ninja)
#   - CMake (>= min, via Kitware repo if needed)
#   - clang tools (clangd/format/tidy)
#   - Conan (via pipx)
#   - Bootlin aarch64 toolchain (Jetson r36)
#   - QEMU + binfmt for ARM64 chroot execution
#
# NOTE: On native Ubuntu, qemu-user-static postinst auto-registers
#       binfmt handlers via update-binfmts. No manual /proc write needed.
#
# Stages:
#   apt-update | apt-packages | cmake | clang | conan | bootlin | binfmt | versions
#
# Usage:
#   ./scripts/bootstrap-toolchain/ubuntu.sh
#   ./scripts/bootstrap-toolchain/ubuntu.sh --stage bootlin --force
#   ./scripts/bootstrap-toolchain/ubuntu.sh --stage binfmt --force
#   ./scripts/bootstrap-toolchain/ubuntu.sh --list-stages
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

apt_update()  { run_as_root apt-get update -y; }
apt_install() { run_as_root apt-get install -y --no-install-recommends "$@"; }

version_ge() { dpkg --compare-versions "$2" ge "$1"; }

repo_root() { (cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); }
hash_stdin() { sha256sum | awk '{print $1}'; }

REPO_ROOT="$(repo_root)"
CACHE_DIR="${REPO_ROOT}/.cache"
STAMPS_DIR="${CACHE_DIR}/stamps/bootstrap-toolchain-ubuntu"
mkdir -p "${STAMPS_DIR}"

stamp_file() { echo "${STAMPS_DIR}/$1.stamp"; }
fp_match()   { [[ -f "$(stamp_file "$1")" ]] && grep -qx "$2" "$(stamp_file "$1")"; }
fp_write()   { printf "%s\n" "$2" >"$(stamp_file "$1")"; }

# ---------------------------
# Config
# ---------------------------

APT_PACKAGES=(
  # Sysroot bootstrap prereqs (host-side)
  ca-certificates curl tar bzip2 xz-utils rsync dpkg coreutils

  # Needed for ARM chroot execution
  # Ubuntu 24.04+ ships qemu-user-binfmt (static-pie) instead of qemu-user-static
  qemu-user-binfmt binfmt-support

  # General dev tools
  build-essential ninja-build pkg-config git unzip gdb make
  python3 python3-venv python3-pip pipx ccache

  # For Kitware repo setup (cmake stage uses these)
  gpg wget
)

CLANG_PACKAGES=(clangd clang-format clang-tidy)
CMAKE_MIN_VERSION="3.24.0"

# Bootlin toolchain (Jetson r36 series)
BOOTLIN_URL="https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v3.0/toolchain/aarch64--glibc--stable-2022.08-1.tar.bz2"
BOOTLIN_TOOLCHAINS_DIR="${CACHE_DIR}/toolchains"
BOOTLIN_INSTALL_DIR="${BOOTLIN_TOOLCHAINS_DIR}/bootlin-jetson-r36"
BOOTLIN_ENV_SH="${BOOTLIN_INSTALL_DIR}/env.sh"

# ---------------------------
# Stages
# ---------------------------

stage_apt_update() {
  log "[apt-update] Updating apt indexes..."
  apt_update
}

stage_apt_packages() {
  local fp
  fp="$(printf "%s\n" "${APT_PACKAGES[@]}" | hash_stdin)"
  if fp_match "apt-packages" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[apt-packages] Up to date."
    return 0
  fi
  log "[apt-packages] Installing host packages..."
  apt_install "${APT_PACKAGES[@]}"
  fp_write "apt-packages" "$fp"
}

stage_cmake() {
  local fp
  fp="$(printf "min=%s\n" "$CMAKE_MIN_VERSION" | hash_stdin)"

  if fp_match "cmake" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if need_cmd cmake; then
      local have
      have="$(cmake --version | head -n 1 | awk '{print $3}')"
      if version_ge "$CMAKE_MIN_VERSION" "$have"; then
        log "[cmake] Up to date (found ${have})."
        return 0
      fi
    fi
  fi

  local have="0"
  if need_cmd cmake; then have="$(cmake --version | head -n 1 | awk '{print $3}')"; fi
  if [[ "$have" != "0" ]] && version_ge "$CMAKE_MIN_VERSION" "$have"; then
    log "[cmake] CMake already >= ${CMAKE_MIN_VERSION} (found ${have})."
    fp_write "cmake" "$fp"
    return 0
  fi

  log "[cmake] Installing/Upgrading CMake via Kitware repo..."
  apt_install ca-certificates gpg wget

  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null \
    | gpg --dearmor \
    | run_as_root tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null

  local codename
  codename="$(. /etc/os-release && echo "$VERSION_CODENAME")"

  echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ ${codename} main" \
    | run_as_root tee /etc/apt/sources.list.d/kitware.list >/dev/null

  apt_update
  apt_install cmake
  fp_write "cmake" "$fp"
}

stage_clang() {
  local fp
  fp="$(printf "%s\n" "${CLANG_PACKAGES[@]}" | hash_stdin)"
  if fp_match "clang" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[clang] Up to date."
    return 0
  fi
  log "[clang] Installing clang tools..."
  apt_install "${CLANG_PACKAGES[@]}"
  fp_write "clang" "$fp"
}

stage_conan() {
  local fp
  fp="$(printf "pipx-conan\n" | hash_stdin)"
  if fp_match "conan" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    log "[conan] Up to date."
    return 0
  fi
  log "[conan] Ensuring Conan installed via pipx..."
  if ! need_cmd conan; then
    need_cmd pipx || die "pipx not installed (should be in APT_PACKAGES)."
    pipx install conan
  fi
  fp_write "conan" "$fp"
}

# Pick the REAL underlying compiler binaries (avoid toolchain-wrapper)
bootlin_fix_compiler_symlinks() {
  local bin="${BOOTLIN_INSTALL_DIR}/bin"
  [[ -d "$bin" ]] || die "[bootlin] Missing bin dir: $bin"

  local gcc_real=""
  if [[ -x "$bin/aarch64-buildroot-linux-gnu-gcc-11.3.0.br_real" ]]; then
    gcc_real="$bin/aarch64-buildroot-linux-gnu-gcc-11.3.0.br_real"
  elif [[ -x "$bin/aarch64-buildroot-linux-gnu-gcc.br_real" ]]; then
    gcc_real="$bin/aarch64-buildroot-linux-gnu-gcc.br_real"
  else
    gcc_real="$(ls -1 "$bin"/aarch64-buildroot-linux-gnu-gcc-*.br_real 2>/dev/null | head -n 1 || true)"
  fi
  [[ -n "$gcc_real" && -x "$gcc_real" ]] || die "[bootlin] Could not find gcc .br_real under: $bin"

  local gxx_real=""
  if [[ -x "$bin/aarch64-buildroot-linux-gnu-g++.br_real" ]]; then
    gxx_real="$bin/aarch64-buildroot-linux-gnu-g++.br_real"
  else
    gxx_real="$(ls -1 "$bin"/aarch64-buildroot-linux-gnu-g++*.br_real 2>/dev/null | head -n 1 || true)"
  fi
  [[ -n "$gxx_real" && -x "$gxx_real" ]] || die "[bootlin] Could not find g++ .br_real under: $bin"

  ln -sf "$(basename "$gcc_real")" "$bin/aarch64-buildroot-linux-gnu-gcc"
  ln -sf "$(basename "$gxx_real")" "$bin/aarch64-buildroot-linux-gnu-g++"

  if [[ -L "$bin/aarch64-buildroot-linux-gnu-c++" ]]; then
    ln -sf "aarch64-buildroot-linux-gnu-g++" "$bin/aarch64-buildroot-linux-gnu-c++"
  else
    ln -sf "aarch64-buildroot-linux-gnu-g++" "$bin/aarch64-buildroot-linux-gnu-c++"
  fi

  cat > "$BOOTLIN_ENV_SH" <<EOF
export BOOTLIN_JETSON_R36_ROOT="${BOOTLIN_INSTALL_DIR}"
export PATH="\${BOOTLIN_JETSON_R36_ROOT}/bin:\${PATH}"
export CC="aarch64-buildroot-linux-gnu-gcc"
export CXX="aarch64-buildroot-linux-gnu-g++"
EOF
}

stage_bootlin() {
  local fp
  fp="$(printf "url=%s\n" "$BOOTLIN_URL" | hash_stdin)"

  if fp_match "bootlin" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if [[ -d "${BOOTLIN_INSTALL_DIR}/bin" ]]; then
      log "[bootlin] Up to date (repairing symlinks just in case)..."
      bootlin_fix_compiler_symlinks
      return 0
    fi
  fi

  mkdir -p "$BOOTLIN_TOOLCHAINS_DIR"

  local tmp="/tmp/bootlin-jetson-r36.tar.bz2"
  log "[bootlin] Downloading toolchain..."
  curl -L --fail --retry 3 -o "$tmp" "$BOOTLIN_URL"

  log "[bootlin] Extracting..."
  tar -xjf "$tmp" -C "$BOOTLIN_TOOLCHAINS_DIR"
  rm -f "$tmp"

  local extracted
  extracted="$(find "$BOOTLIN_TOOLCHAINS_DIR" -maxdepth 1 -type d -name 'aarch64--glibc--stable-*' | head -n 1 || true)"
  [[ -n "$extracted" ]] || die "[bootlin] Could not find extracted dir under $BOOTLIN_TOOLCHAINS_DIR"

  rm -rf "$BOOTLIN_INSTALL_DIR"
  mv "$extracted" "$BOOTLIN_INSTALL_DIR"

  bootlin_fix_compiler_symlinks

  fp_write "bootlin" "$fp"
  log "[bootlin] Installed at: $BOOTLIN_INSTALL_DIR"
}

stage_binfmt() {
  local fp
  fp="$(printf "binfmt=qemu-aarch64-ubuntu\n" | hash_stdin)"

  if fp_match "binfmt" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 && -x /usr/bin/qemu-aarch64-static ]]; then
      log "[binfmt] Up to date."
      return 0
    fi
  fi

  # Ubuntu 24.04+ ships /usr/bin/qemu-aarch64 (static-pie, no -static suffix)
  [[ -x /usr/bin/qemu-aarch64 ]] || die \
    "[binfmt] /usr/bin/qemu-aarch64 not found. Run: --stage apt-packages --force"

  # binfmt_misc is registered by systemd-binfmt on native Ubuntu via
  # /usr/lib/binfmt.d/qemu-aarch64.conf — just verify it's active.
  [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] || die \
    "[binfmt] binfmt entry missing. Try: sudo systemctl restart systemd-binfmt"

  # NVIDIA apply_binaries.sh copies /usr/bin/qemu-aarch64-static into the rootfs.
  # Create a compat symlink so that path exists.
  if [[ ! -x /usr/bin/qemu-aarch64-static ]]; then
    log "[binfmt] Creating /usr/bin/qemu-aarch64-static compat symlink..."
    run_as_root ln -sf qemu-aarch64 /usr/bin/qemu-aarch64-static
  fi

  fp_write "binfmt" "$fp"
  log "[binfmt] Ready: /proc/sys/fs/binfmt_misc/qemu-aarch64 -> /usr/bin/qemu-aarch64"
  log "[binfmt] Compat: /usr/bin/qemu-aarch64-static -> qemu-aarch64"
}

stage_versions() {
  log "[versions] Tool versions:"
  gcc --version | head -n 1 || true
  g++ --version | head -n 1 || true
  cmake --version | head -n 1 || true
  ninja --version 2>/dev/null || true
  clangd --version | head -n 1 2>/dev/null || true
  conan --version 2>/dev/null || true
  qemu-aarch64-static --version 2>/dev/null | head -n 1 || true

  local bin="${BOOTLIN_INSTALL_DIR}/bin"
  if [[ -x "$bin/aarch64-buildroot-linux-gnu-gcc" ]]; then
    log "[versions] Bootlin gcc: $("$bin/aarch64-buildroot-linux-gnu-gcc" --version | head -n 1 || true)"
  else
    warn "[versions] Missing: $bin/aarch64-buildroot-linux-gnu-gcc"
  fi
  if [[ -x "$bin/aarch64-buildroot-linux-gnu-g++" ]]; then
    log "[versions] Bootlin g++: $("$bin/aarch64-buildroot-linux-gnu-g++" --version | head -n 1 || true)"
  else
    warn "[versions] Missing: $bin/aarch64-buildroot-linux-gnu-g++"
  fi

  if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
    log "[versions] binfmt qemu-aarch64: present"
  else
    warn "[versions] binfmt qemu-aarch64: MISSING (run --stage binfmt)"
  fi

  log "[versions] Bootlin env:"
  printf "  source %s\n" "$BOOTLIN_ENV_SH"
}

# ---------------------------
# Stage runner + CLI
# ---------------------------

ALL_STAGES=(apt-update apt-packages cmake clang conan bootlin binfmt versions)

list_stages() { printf "%s\n" "${ALL_STAGES[@]}"; }

run_stage() {
  case "$1" in
    apt-update)   stage_apt_update ;;
    apt-packages) stage_apt_packages ;;
    cmake)        stage_cmake ;;
    clang)        stage_clang ;;
    conan)        stage_conan ;;
    bootlin)      stage_bootlin ;;
    binfmt)       stage_binfmt ;;
    versions)     stage_versions ;;
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

  cat <<EOF

Bootlin toolchain:
  source "${BOOTLIN_ENV_SH}"

Stable tool names (these MUST point to *.br_real):
  ${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-gcc
  ${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-g++

binfmt:
  /proc/sys/fs/binfmt_misc/qemu-aarch64 should exist
  If missing after reboot:
    $0 --stage binfmt --force

EOF
}

main "$@"
