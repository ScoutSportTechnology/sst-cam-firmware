#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# bootstrap-jetson-r36.5-toolchain.sh
#
# Jetson-native dev tool bootstrap:
#   - Build uses default system toolchain (gcc/g++)
#   - Adds Ninja + CMake (>= 3.24 via Kitware repo if needed)
#   - Adds clang tooling for dev UX only (clangd/format/tidy)
#   - Installs Conan via pipx (isolated, sane)
#
# Tested targets: Jetson Linux r36.x (JetPack 6.x), Ubuntu 22.04
# ============================================================

log() { printf "\n\033[1m%s\033[0m\n" "$*"; }
warn() { printf "\n\033[33mWARN:\033[0m %s\n" "$*"; }
die() { printf "\n\033[31mERROR:\033[0m %s\n" "$*"; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }

version_ge() {
  # usage: version_ge "3.24.0" "3.22.1" -> false
  # returns 0 if $2 >= $1
  local want="$1"
  local have="$2"
  dpkg --compare-versions "$have" ge "$want"
}

is_ubuntu() {
  [[ -r /etc/os-release ]] && grep -qi '^ID=ubuntu' /etc/os-release
}

ubuntu_codename() {
  . /etc/os-release
  echo "${VERSION_CODENAME:-}"
}

ubuntu_version() {
  . /etc/os-release
  echo "${VERSION_ID:-unknown}"
}

ensure_sudo() {
  if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    if ! need_cmd sudo; then
      die "sudo not found. Install sudo or run this script as root."
    fi
  fi
}

apt_install() {
  # shellcheck disable=SC2068
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    apt-get install -y --no-install-recommends $@
  else
    sudo apt-get install -y --no-install-recommends $@
  fi
}

apt_update() {
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    apt-get update -y
  else
    sudo apt-get update -y
  fi
}

install_cmake_ge_324() {
  local want="3.24.0"
  local have="0"

  if need_cmd cmake; then
    have="$(cmake --version | head -n 1 | awk '{print $3}')"
  fi

  if [[ "$have" != "0" ]] && version_ge "$want" "$have"; then
    log "CMake already >= ${want} (found ${have})."
    return 0
  fi

  if ! is_ubuntu; then
    warn "Not Ubuntu; skipping Kitware repo setup. Install CMake >= ${want} manually."
    return 0
  fi

  local codename
  codename="$(ubuntu_codename)"
  if [[ -z "$codename" ]]; then
    warn "Could not detect Ubuntu codename; skipping Kitware repo setup. Install CMake >= ${want} manually."
    return 0
  fi

  log "Installing/Upgrading CMake via Kitware APT repo (need >= ${want}, currently ${have})..."

  apt_install ca-certificates gpg wget

  # Keyring
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null \
      | gpg --dearmor \
      > /usr/share/keyrings/kitware-archive-keyring.gpg
  else
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null \
      | gpg --dearmor \
      | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
  fi

  # Repo list
  local list_line="deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ ${codename} main"
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    echo "$list_line" > /etc/apt/sources.list.d/kitware.list
  else
    echo "$list_line" | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
  fi

  apt_update
  apt_install cmake

  local newver
  newver="$(cmake --version | head -n 1 | awk '{print $3}')"
  if ! version_ge "$want" "$newver"; then
    die "CMake upgrade failed. Need >= ${want}, got ${newver}. Check Kitware repo availability for ${codename}."
  fi

  log "CMake upgraded to ${newver}."
}

print_versions() {
  log "Tool versions:"
  if need_cmd gcc; then gcc --version | head -n 1; else echo "gcc: not found"; fi
  if need_cmd g++; then g++ --version | head -n 1; else echo "g++: not found"; fi
  if need_cmd cmake; then cmake --version | head -n 1; else echo "cmake: not found"; fi
  if need_cmd ninja; then ninja --version | sed 's/^/ninja: /'; else echo "ninja: not found"; fi
  if need_cmd clangd; then clangd --version | head -n 1; else echo "clangd: not found"; fi
  if need_cmd clang-format; then clang-format --version | head -n 1; else echo "clang-format: not found"; fi
  if need_cmd clang-tidy; then clang-tidy --version | head -n 1; else echo "clang-tidy: not found"; fi
  if need_cmd conan; then conan --version | sed 's/^/conan: /'; else echo "conan: not found"; fi
}

main() {
  ensure_sudo

  if ! is_ubuntu; then
    warn "This script expects Ubuntu (Jetson Linux r36.x is typically Ubuntu 22.04). Proceeding anyway."
  else
    local ver
    ver="$(ubuntu_version)"
    if [[ "$ver" != "22.04" ]]; then
      warn "Detected Ubuntu $ver (expected 22.04 for Jetson r36.x). Proceeding anyway."
    fi
  fi

  log "Updating apt index..."
  apt_update

  log "Installing core build/dev tools (gcc/g++, ninja, pkg-config, git, python)..."
  # NOTE: We do NOT install cmake here yet because Ubuntu 22.04 gives 3.22.
  apt_install \
    build-essential \
    ninja-build \
    pkg-config \
    git \
    ca-certificates \
    curl \
    unzip \
    tar \
    gdb \
    python3 \
    python3-venv \
    python3-pip \
    pipx

  # Ensure CMake >= 3.24 (needed by your conan_provider.cmake as currently written)
  install_cmake_ge_324

  # Make pipx available in PATH for this user (idempotent)
  if need_cmd pipx; then
    pipx ensurepath >/dev/null 2>&1 || true
    export PATH="$HOME/.local/bin:$PATH"
  fi

  log "Installing clang tooling (clangd/format/tidy) for editor + linting (not for compiling)..."
  apt_install clangd clang-format clang-tidy

  log "Installing Conan via pipx (isolated)..."
  if need_cmd conan; then
    log "Conan already installed: $(conan --version)"
  else
    pipx install conan
  fi

  log "Installing extra C/C++ helpers (optional but useful):"
  apt_install \
    ccache \
    lsb-release

  log "Bootstrap complete."
  print_versions

  cat <<'EOF'

Notes / Defaults:
- Your *compiler* remains the system default: gcc/g++.
- clangd/clang-format/clang-tidy are installed for IDE/LSP/format/tidy only.
- Conan is installed via pipx (recommended on Ubuntu to avoid messing with system Python).

EOF
}

main "$@"