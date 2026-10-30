#!/usr/bin/env bash
set -euo pipefail

case "${1:-}" in
lint)
  scope="${2:-full}"
  if [ "$scope" = changed ]; then
    cmake --preset test
    cmake --build --preset test --target sst_proto
    source scripts/tidy-args.sh
    files="$(git diff --name-only "${BASE:-origin/development}" -- src tests; git diff --name-only -- src tests; git ls-files --others --exclude-standard -- src tests)"
  elif [ "$scope" = full ]; then
    files="$(find src tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' -o -name '*.h' \) -not -path '*/_old/*')"
  else
    exit 2
  fi
  if [ "$scope" = full ] && [ -z "$files" ]; then exit 1; fi
  if printf '%s\n' "$files" | grep -Eq '\.(cpp|hpp|cc|h)$'; then
    printf '%s\n' "$files" | grep -E '\.(cpp|hpp|cc|h)$' | xargs -r clang-format-18 --dry-run --Werror
  fi
  if [ "$scope" = changed ]; then
    if printf '%s\n' "$files" | grep -Eq '\.(cpp|cc)$'; then
      printf '%s\n' "$files" | grep -E '\.(cpp|cc)$' | xargs -r -P "$(nproc)" -n 1 clang-tidy -p build/test "${TIDY_EXTRA_ARGS[@]}"
    fi
  else
    scripts/ci/tidy-run.sh full
  fi
  scripts/test-check-floor-nolints.sh
  scripts/check-floor-nolints.sh "${BASE:-origin/development}"
  shellcheck -x scripts/ci/*.sh scripts/*.sh deploy/*.sh
  scripts/ci/check-proto-pin.sh "${BASE_REF:-development}"
  ;;
compile)
  cmake --preset debug
  cmake --build --preset debug -j "$(nproc)"
  ;;
test)
  cmake --preset test
  cmake --build --preset test -j "$(nproc)"
  QEMU_LD_PREFIX=/l4t/targetfs ctest --preset test -j "$(nproc)" -LE hardware --output-on-failure
  scripts/ci/test-tidy-run.sh
  deploy/install-test.sh
  ;;
build)
  cmake --preset release
  cmake --build --preset release -j "$(nproc)"
  mkdir -p dist
  cp build/release/bin/sst_cam_firmware dist/sst_cam_firmware-aarch64
  ;;
ci)
  cmake --preset test
  cmake --preset debug
  scripts/ci/recipes.sh lint & lint_pid=$!
  scripts/ci/recipes.sh compile & compile_pid=$!
  lint_status=0
  compile_status=0
  wait "$lint_pid" || lint_status=$?
  wait "$compile_pid" || compile_status=$?
  test "$lint_status" -eq 0
  test "$compile_status" -eq 0
  scripts/ci/recipes.sh test
  ;;
*)
  exit 2
  ;;
esac
