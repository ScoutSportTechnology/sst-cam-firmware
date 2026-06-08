#!/usr/bin/env bash
# Runs once after the container is created (after onCreate), as the remote user.
# Mark the bind-mounted workspace as a safe git directory — the container UID can
# differ from the host's, which otherwise trips git's dubious-ownership guard.
set -euo pipefail

git config --global --add safe.directory /workspace
echo "[post-create] marked /workspace as a safe git directory"

# Pull in the sst-cam-proto wire contract (proto/) so codegen has its schema.
# The workspace is bind-mounted at runtime, so the submodule must be initialised
# inside the container rather than baked into the image.
git -C /workspace submodule update --init --recursive
echo "[post-create] initialised git submodules (proto/)"
