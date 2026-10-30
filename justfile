# SST Cam firmware — task runner.
#
# Recipes run inside the container or use the pinned compiler image from the host.
#
# The firmware cross-compiles to aarch64 (Jetson). Pure-logic unit tests run on
# the x86 host under qemu (QEMU_LD_PREFIX points at the sysroot loader);
# hardware-bound tests (real IMX477 / BlueZ / radio / live GStreamer) only pass
# on-device and are expected to fail in the container.
#
# Host requires `just` + Docker; compiler-image/VERSION pins the image.

set shell := ["bash", "-uc"]

# aarch64 sysroot loader prefix — lets qemu run the cross-compiled test binary.
qemu_prefix := "/l4t/targetfs"
test_bin := "./build/test/bin/sst_cam_firmware_tests"

# List recipes.
default:
    @just --list

# --- container lifecycle (host-side) --------------------------------------

# Check the pinned compiler image is available locally.
up:
    @docker image inspect "ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:$(tr -d '[:space:]' < compiler-image/VERSION)" >/dev/null

# Open an interactive shell in the pinned compiler image.
shell:
    @image="ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:$(tr -d '[:space:]' < compiler-image/VERSION)"; \
      git_dir="$(git rev-parse --path-format=absolute --git-common-dir)"; \
      docker image inspect "$image" >/dev/null || { echo "Pinned compiler image not present: $image" >&2; exit 1; }; \
      test "$(df -BG --output=avail / | tr -dc '0-9')" -ge 10 || { echo 'Need 10 GB free before running compiler image' >&2; exit 1; }; \
      docker run --rm -it --user root -v "{{justfile_directory()}}":"{{justfile_directory()}}" -v "$git_dir":"$git_dir":ro \
        -w "{{justfile_directory()}}" "$image" bash

# --- build (auto native-or-delegated) -------------------------------------

# Configure + build the debug binary (default dev-loop build).
build-debug-firmware:
    @just _run "cmake --preset debug && cmake --build --preset debug"

# Configure + build the release binary (the deployable artifact).
build-release-firmware:
    @just _run "cmake --preset release && cmake --build --preset release"

# Configure + build the test binary (GTest via Conan).
build-test:
    @just _run "cmake --preset test && cmake --build --preset test"

# --- test -----------------------------------------------------------------

test:
    @just _run "scripts/ci/recipes.sh test"

test-device:
    @just _run 'ctest --preset test -L hardware -j "$(nproc)" --output-on-failure'

# Run the already-built suite without rebuilding (faster iteration).
test-only *args:
    @just _run "QEMU_LD_PREFIX={{qemu_prefix}} {{test_bin}} {{args}}"

# Run via ctest (shows the CTest test list / timings).
ctest:
    @just _run "QEMU_LD_PREFIX={{qemu_prefix}} ctest --preset test --output-on-failure"

# --- CI parity ------------------------------------------------------------

# The exact gate CI runs: format-check + tidy + build-test + ctest. Same
# entrypoint locally and in .github/workflows/ci.yml — keep the two in sync.
ci-check:
    @just _run "set -euo pipefail && \
        srcs=\$(find src tests \\( -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' -o -name '*.h' \\) -not -path '*/_old/*') && \
        printf '%s\\n' \"\$srcs\" | xargs clang-format --dry-run --Werror && \
        cmake --preset test && \
        tus=\$(find src tests \\( -name '*.cpp' -o -name '*.cc' \\) -not -path '*/_old/*') && \
        printf '%s\\n' \"\$tus\" | xargs clang-tidy -p build/test --warnings-as-errors='*' && \
        cmake --build --preset test && \
        QEMU_LD_PREFIX={{qemu_prefix}} ctest --preset test --output-on-failure"

lint scope="full":
    @just _run "scripts/ci/recipes.sh lint {{scope}}"

fmt:
    @just _run 'scripts/fix.sh --all'

compile:
    @just _run "scripts/ci/recipes.sh compile"

build:
    @just _run "scripts/ci/recipes.sh build"

ci:
    @just _run "scripts/ci/recipes.sh ci"

# --- local deploy loop ----------------------------------------------------

# Local validation loop — cross-build the release binary and install it straight
# to a Jetson over ssh, WITHOUT cutting a release or minting a tag. The build
# runs in the container; scp/ssh run on the HOST (which holds your ssh keys /
# ~/.ssh/config), so you need `ssh`/`scp` on the host PATH and key-based access
# to the device. Validate here; only push (and let CI mint a tag) once it works.
#
# Usage: just deploy-firmware user@jetson.local
#        just deploy-firmware jetson.local --no-camera   # extra install.sh flags pass through
deploy-firmware HOST *flags: build-release-firmware
    @BIN="build/release/bin/sst_cam_firmware"; \
     [ -f "$BIN" ] || { echo "binary not found: $BIN (build-release failed?)" >&2; exit 1; }; \
     command -v scp >/dev/null || { echo "scp not found on host PATH" >&2; exit 1; }; \
     echo "Copying ${BIN} + install.sh to {{HOST}} ..."; \
     scp "$BIN" deploy/install.sh "{{HOST}}:/tmp/" || exit 1; \
     echo "Running install.sh --binary on {{HOST}} (sudo) ..."; \
     ssh -t "{{HOST}}" "sudo bash /tmp/install.sh --binary /tmp/sst_cam_firmware {{flags}}"

# --- housekeeping ---------------------------------------------------------

# Remove build trees (config + objects; the Conan cache is untouched).
clean:
    @just _run "rm -rf build"

# --- internals ------------------------------------------------------------

# Run CMD natively inside a container, otherwise in the pinned compiler image.
[private]
_run +cmd:
    @if [ -f /.dockerenv ]; then \
        bash -lc "{{cmd}}"; \
    else \
        image="ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:$(tr -d '[:space:]' < compiler-image/VERSION)"; \
        git_dir="$(git rev-parse --path-format=absolute --git-common-dir)"; \
        docker image inspect "$image" >/dev/null || { echo "Pinned compiler image not present: $image" >&2; exit 1; }; \
        test "$(df -BG --output=avail / | tr -dc '0-9')" -ge 10 || { echo 'Need 10 GB free before running compiler image' >&2; exit 1; }; \
        docker run --rm --user root -v "{{justfile_directory()}}":"{{justfile_directory()}}" -v "$git_dir":"$git_dir":ro \
            -w "{{justfile_directory()}}" -e BASE="${BASE:-origin/development}" -e BASE_REF="${BASE_REF:-development}" -e SST_RELEASE_TAG="${SST_RELEASE_TAG:-}" "$image" \
            bash -lc 'set -euo pipefail; if [[ "$1" == *"recipes.sh lint"* || "$1" == *"recipes.sh ci"* ]]; then apt-get update -qq; apt-get install -y -qq --no-install-recommends clang-tidy clang-format shellcheck >/dev/null; fi; exec bash -lc "$1"' _ "{{cmd}}"; \
    fi
