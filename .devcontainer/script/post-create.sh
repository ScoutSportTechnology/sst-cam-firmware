#!/usr/bin/env bash
# Runs once after the container is created (after onCreate), as the remote user.
# Mark the bind-mounted workspace as a safe git directory — the container UID can
# differ from the host's, which otherwise trips git's dubious-ownership guard.
set -euo pipefail

git config --global --add safe.directory /workspace
echo "[post-create] marked /workspace as a safe git directory"
