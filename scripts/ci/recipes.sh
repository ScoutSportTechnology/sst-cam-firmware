#!/usr/bin/env bash
set -euo pipefail

# Fix for "fatal: detected dubious ownership in repository" when the checkout
# is owned by a different UID than the container user (root).
export GIT_CONFIG_COUNT=1
export GIT_CONFIG_KEY_0=safe.directory
export GIT_CONFIG_VALUE_0='*'

# Detect ccache availability and set up launcher flags for CMake
setup_ccache() {
  if command -v ccache >/dev/null 2>&1; then
    export CCACHE_DIR="${CCACHE_DIR:-/root/.ccache}"
    export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-20G}"
    export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$(pwd)}"
    export CCACHE_NOHASHDIR=1
    export CCACHE_COMPILERCHECK=content
    export CMAKE_C_COMPILER_LAUNCHER=ccache
    export CMAKE_CXX_COMPILER_LAUNCHER=ccache
    ccache -M "$CCACHE_MAXSIZE" >/dev/null 2>&1 || true
    ccache -z >/dev/null 2>&1 || true
  fi
}

# Run cmake --preset with flock serialization on the shared Conan cache.
# This prevents concurrent Conan installs from multiple worktrees corrupting the cache.
run_cmake_preset() {
  local preset="$1"
  local lock_file="${CONAN_HOME:-/root/.conan2}/.cmake_configure.lock"
  local lock_fd=200

  # Ensure the lock directory exists
  mkdir -p "$(dirname "$lock_file")"

  # Prepare ccache args if available
  local ccache_args=()
  if command -v ccache >/dev/null 2>&1; then
    ccache_args=(
      -DCMAKE_C_COMPILER_LAUNCHER=ccache
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
  fi

  # Only serialize the first configure (which runs Conan); subsequent builds are parallel-safe.
  if [ ! -f "build/$preset/CMakeCache.txt" ]; then
    eval "exec $lock_fd>\"$lock_file\""
    flock "$lock_fd"
    cmake --preset "$preset" "${ccache_args[@]}"
    flock -u "$lock_fd"
    eval "exec $lock_fd>&-"
  else
    # Reconfigure with ccache args to ensure they're picked up (CMake caches these values)
    cmake --preset "$preset" "${ccache_args[@]}"
  fi
}

case "${1:-}" in
lint)
  scope="${2:-full}"
  if [ "$scope" = changed ]; then
    run_cmake_preset test
    cmake --build --preset test --target sst_proto
    source scripts/tidy-args.sh

    # Get changed files (excluding deleted)
    changed_files="$(git diff --name-only --diff-filter=d "${BASE:-origin/development}" -- src tests; git diff --name-only --diff-filter=d -- src tests; git ls-files --others --exclude-standard -- src tests)"

    # Build list of translation units to lint
    tu_files=""

    # Add changed .cpp/.cc files directly
    tu_files="$(printf '%s\n' "$changed_files" | grep -E '\.(cpp|cc)$' || true)"

    # For each changed header under src/ or tests/, find .cpp/.cc that include it
    changed_headers="$(printf '%s\n' "$changed_files" | grep -E '\.(hpp|h)$' | grep -E '^(src|tests)/' || true)"
    if [ -n "$changed_headers" ]; then
      while IFS= read -r header; do
        [ -n "$header" ] || continue
        # Find includes by matching the include path
        include_name="${header#src/}"
        include_name="${include_name#tests/}"
        # Find .cpp/.cc files that include this header
        matching="$(grep -rl "#include.*${include_name}" src tests --include='*.cpp' --include='*.cc' 2>/dev/null || true)"
        if [ -n "$matching" ]; then
          tu_files+="$(printf '\n%s\n' "$matching")"
        else
          # Fallback: try matching by basename
          basename_header="$(basename "$header")"
          matching="$(grep -rl "#include.*${basename_header}" src tests --include='*.cpp' --include='*.cc' 2>/dev/null || true)"
          if [ -n "$matching" ]; then
            tu_files+="$(printf '\n%s\n' "$matching")"
          fi
        fi
      done <<<"$changed_headers"
    fi

    # Deduplicate
    tu_files="$(printf '%s\n' "$tu_files" | sort -u | sed '/^$/d')"

    # Fall back to full tidy if too many TUs or config files changed
    config_changed="$(printf '%s\n' "$changed_files" | grep -E '^(\.clang-tidy|scripts/tidy-args\.sh|scripts/ci/tidy-run\.sh|CMakeLists\.txt|cmake/.*|CMakePresets\.json|conanfile\.py)$' || true)"
    tu_count="$(printf '%s\n' "$tu_files" | grep -c '^' || echo 0)"

    if [ -n "$config_changed" ] || [ "$tu_count" -gt 150 ]; then
      echo "lint: falling back to full tidy (config_changed=$config_changed, tu_count=$tu_count)"
      scripts/ci/tidy-run.sh full
    else
      if [ -n "$tu_files" ]; then
        printf '%s\n' "$tu_files" | xargs -r -P "$(nproc)" -n 1 clang-tidy -p build/test "${TIDY_EXTRA_ARGS[@]}"
      fi
    fi
  elif [ "$scope" = full ]; then
    files="$(find src tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' -o -name '*.h' \) -not -path '*/_old/*')"
    if [ -z "$files" ]; then exit 1; fi
    scripts/ci/tidy-run.sh full
  else
    exit 2
  fi

  # Common checks for both scopes
  if [ "$scope" = changed ]; then
    if [ -n "$tu_files" ] && printf '%s\n' "$tu_files" | grep -Eq '\.(cpp|hpp|cc|h)$'; then
      printf '%s\n' "$tu_files" | grep -E '\.(cpp|hpp|cc|h)$' | xargs -r clang-format-18 --dry-run --Werror
    fi
  elif printf '%s\n' "$files" | grep -Eq '\.(cpp|hpp|cc|h)$'; then
    printf '%s\n' "$files" | grep -E '\.(cpp|hpp|cc|h)$' | xargs -r clang-format-18 --dry-run --Werror
  fi
  scripts/test-check-floor-nolints.sh
  scripts/check-floor-nolints.sh "${BASE:-origin/development}"
  shellcheck -x scripts/ci/*.sh scripts/*.sh deploy/*.sh
  scripts/ci/check-proto-pin.sh "${BASE_REF:-development}"
  ;;
compile)
  setup_ccache
  run_cmake_preset debug
  cmake --build --preset debug -j "$(nproc)"
  ;;
test)
  setup_ccache
  run_cmake_preset test
  cmake --build --preset test -j "$(nproc)"
  test_filter="${2:-}"
  if [ -n "$test_filter" ]; then
    SST_TEST_FILTER="$test_filter" QEMU_LD_PREFIX=/l4t/targetfs ctest --preset test -j "$(nproc)" -LE hardware --output-on-failure
  else
    QEMU_LD_PREFIX=/l4t/targetfs ctest --preset test -j "$(nproc)" -LE hardware --output-on-failure
  fi
  scripts/ci/test-tidy-run.sh
  deploy/install-test.sh
  ;;
build)
  setup_ccache
  run_cmake_preset release
  cmake --build --preset release -j "$(nproc)"
  mkdir -p dist
  cp build/release/bin/sst_cam_firmware dist/sst_cam_firmware-aarch64
  ;;
ci)
  # ci [changed]
  scope="${2:-}"
  if [ "$scope" = "changed" ]; then
    # Changed scope: lint changed in parallel with compile, then test
    run_cmake_preset test
    run_cmake_preset debug
    scripts/ci/recipes.sh lint changed & lint_pid=$!
    scripts/ci/recipes.sh compile & compile_pid=$!
    lint_status=0
    compile_status=0
    wait "$lint_pid" || lint_status=$?
    wait "$compile_pid" || compile_status=$?
    test "$lint_status" -eq 0
    test "$compile_status" -eq 0
    scripts/ci/recipes.sh test
  else
    # Full gate (unchanged behavior)
    run_cmake_preset test
    run_cmake_preset debug
    scripts/ci/recipes.sh lint & lint_pid=$!
    scripts/ci/recipes.sh compile & compile_pid=$!
    lint_status=0
    compile_status=0
    wait "$lint_pid" || lint_status=$?
    wait "$compile_pid" || compile_status=$?
    test "$lint_status" -eq 0
    test "$compile_status" -eq 0
    scripts/ci/recipes.sh test
  fi
  ;;
*)
  exit 2
  ;;
esac
