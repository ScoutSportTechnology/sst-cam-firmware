#!/usr/bin/env bash
# Runs once inside the container right after it is created, as the remote user.
# One-time setup: wire the bind-mounted aliases into ~/.bashrc and make sure a
# default Conan profile exists in the (bind-mounted, persistent) cache.
set -euo pipefail

# Source the bind-mounted alias file from ~/.bashrc, exactly once.
SNIPPET='[ -f "$HOME/.bashrc.d/00-aliases.bashrc" ] && source "$HOME/.bashrc.d/00-aliases.bashrc"'
if ! grep -qF '.bashrc.d/00-aliases.bashrc' "${HOME}/.bashrc" 2>/dev/null; then
    printf '\n# devcontainer: source bind-mounted aliases\n%s\n' "${SNIPPET}" >> "${HOME}/.bashrc"
    echo "[on-create] wired ~/.bashrc.d/00-aliases.bashrc into ~/.bashrc"
fi

# Create a default Conan profile if the cache doesn't already carry one.
if ! conan profile path default >/dev/null 2>&1; then
    conan profile detect
    echo "[on-create] created default Conan profile"
else
    echo "[on-create] default Conan profile already present"
fi
