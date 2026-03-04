#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# scripts/bootstrap-toolchain/wsl-ubuntu.sh
#
# Host toolchain bootstrap for WSL Ubuntu (x86_64).
#
# Installs:
#   - base dev tools
#   - CMake (>= min, via Kitware repo if needed)
#   - clang tools (clangd/format/tidy)
#   - Conan (via pipx)
#   - Bootlin aarch64 toolchain (Jetson r36)
#   - QEMU + binfmt for ARM64 chroot execution on WSL
#
# Stages:
#   apt-update | apt-packages | cmake | clang | conan | bootlin | binfmt | versions
#
# Usage:
#   ./scripts/bootstrap-toolchain/wsl-ubuntu.sh
#   ./scripts/bootstrap-toolchain/wsl-ubuntu.sh --stage binfmt --force
#   ./scripts/bootstrap-toolchain/wsl-ubuntu.sh --list-stages
# ============================================================

log()  { printf "\n\033[1m%s\033[0m\n" "$*"; }
warn() { printf "\n\033[33mWARN:\033[0m %s\n" "$*"; }
die()  { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }
is_wsl()   { grep -qi microsoft /proc/version 2>/dev/null; }

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
STAMPS_DIR="${CACHE_DIR}/stamps/bootstrap-toolchain-wsl-ubuntu"
mkdir -p "${STAMPS_DIR}"

stamp_file() { echo "${STAMPS_DIR}/$1.stamp"; }
fp_match()   { [[ -f "$(stamp_file "$1")" ]] && grep -qx "$2" "$(stamp_file "$1")"; }
fp_write()   { printf "%s\n" "$2" >"$(stamp_file "$1")"; }

# ---------------------------
# Config
# ---------------------------

# Host prerequisites + dev essentials
APT_PACKAGES=(
  # Your "sysroot bootstrap prereqs" (host-side)
  ca-certificates curl tar bzip2 xz-utils rsync dpkg coreutils

  # Needed for WSL ARM chroot execution
  qemu-user-static binfmt-support

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
  # gpg/wget are already in APT_PACKAGES but safe to assume
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

bootlin_make_stable_symlinks() {
  local bin="${BOOTLIN_INSTALL_DIR}/bin"
  [[ -d "$bin" ]] || die "[bootlin] Missing bin dir: $bin"

  shopt -s nullglob
  local gcc_candidates=("$bin"/aarch64-buildroot-linux-gnu-gcc-*)
  shopt -u nullglob
  [[ ${#gcc_candidates[@]} -ge 1 ]] || die "[bootlin] No gcc found: $bin/aarch64-buildroot-linux-gnu-gcc-*"
  local gcc_real="${gcc_candidates[0]}"

  local cxx_real=""
  if [[ -x "$bin/aarch64-buildroot-linux-gnu-g++" ]]; then
    cxx_real="$bin/aarch64-buildroot-linux-gnu-g++"
  elif [[ -x "$bin/aarch64-buildroot-linux-gnu-c++" ]]; then
    cxx_real="$bin/aarch64-buildroot-linux-gnu-c++"
  elif [[ -x "$bin/aarch64-linux-g++" ]]; then
    cxx_real="$bin/aarch64-linux-g++"
  else
    die "[bootlin] No C++ compiler found."
  fi

  ln -sf "$(basename "$gcc_real")" "$bin/aarch64-buildroot-linux-gnu-gcc"
  ln -sf "$(basename "$cxx_real")" "$bin/aarch64-buildroot-linux-gnu-g++"

  run_as_root mkdir -p "$(dirname "$BOOTLIN_ENV_SH")"
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
    if [[ -x "${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-gcc" ]]; then
      log "[bootlin] Up to date."
      return 0
    fi
    log "[bootlin] Repairing stable symlinks..."
    bootlin_make_stable_symlinks
    return 0
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

  bootlin_make_stable_symlinks

  fp_write "bootlin" "$fp"
  log "[bootlin] Installed at: $BOOTLIN_INSTALL_DIR"
}

stage_binfmt() {
  # This stage exists because WSL does not reliably auto-register qemu-aarch64 in binfmt_misc.
  local fp
  fp="$(printf "binfmt=qemu-aarch64-static\n" | hash_stdin)"

  if fp_match "binfmt" "$fp" && [[ "${FORCE:-0}" -ne 1 ]]; then
    if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
      log "[binfmt] Up to date."
      return 0
    fi
  fi

  log "[binfmt] Ensuring binfmt_misc enabled..."
  run_as_root mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc 2>/dev/null || true

  local status
  status="$(cat /proc/sys/fs/binfmt_misc/status 2>/dev/null || true)"
  [[ "$status" == "enabled" ]] || die "[binfmt] binfmt_misc not enabled (status='$status')"

  need_cmd qemu-aarch64-static || die "[binfmt] qemu-aarch64-static not found. Install qemu-user-static."

  if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
    log "[binfmt] qemu-aarch64 already registered."
    fp_write "binfmt" "$fp"
    return 0
  fi

  if ! is_wsl; then
    warn "[binfmt] Not WSL. If qemu-aarch64 isn't auto-registered, install qemu-user-binfmt or systemd-binfmt config."
  fi

  log "[binfmt] Registering qemu-aarch64 handler (manual)..."
  run_as_root tee /proc/sys/fs/binfmt_misc/register >/dev/null <<'EOF'
:qemu-aarch64:M::\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-aarch64-static:CF
EOF

  [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] || die "[binfmt] Registration failed (qemu-aarch64 entry not created)."

  fp_write "binfmt" "$fp"
  log "[binfmt] Registered: /proc/sys/fs/binfmt_misc/qemu-aarch64"
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

  local stable_gcc="${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-gcc"
  if [[ -x "$stable_gcc" ]]; then
    "$stable_gcc" --version | head -n 1 || true
  else
    warn "[versions] Missing or non-executable: ${stable_gcc}"
    warn "[versions] Run: source ${BOOTLIN_ENV_SH}"
  fi

  if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
    log "[versions] binfmt qemu-aarch64: present"
  else
    warn "[versions] binfmt qemu-aarch64: MISSING (run --stage binfmt)"
  fi
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
  if ! is_wsl; then warn "WSL not detected. Proceeding anyway."; fi

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
  $0 --stage all --force    Force re-run
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

Stable tool names:
  ${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-gcc
  ${BOOTLIN_INSTALL_DIR}/bin/aarch64-buildroot-linux-gnu-g++

WSL binfmt:
  /proc/sys/fs/binfmt_misc/qemu-aarch64 should exist
  Re-run if WSL resets it:
    $0 --stage binfmt --force

EOF
}

main "$@"