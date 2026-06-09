#!/usr/bin/env bash
# Runs on the HOST before the container is created/started.
# Ensure the Conan cache dir exists on the host so the bind mount attaches to a
# real, user-owned directory instead of Docker auto-creating a root-owned one.
set -euo pipefail

mkdir -p "${HOME}/.conan2"
echo "[initialize] host ~/.conan2 ready"
