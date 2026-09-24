#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
#
# The gate is ctest's exit code. It used to be:
#
#     ctest --test-dir "$D" >/tmp/ctest.out 2>&1
#     grep -q "tests passed" /tmp/ctest.out && echo "PASS" || { echo "FAIL"; exit 1; }
#
# which reported PASS on a failing run. ctest prints "50% tests passed, 3 tests
# failed out of 6" when things break, so the substring "tests passed" is present
# in both outcomes and the grep matched either way. The script also had no
# `set -e`, so ctest's own exit code was discarded — the one signal that was
# always correct.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-modeling..."

# Warnings are errors here (-Wall -Wextra -Wpedantic -Werror), so a build
# failure is a real result and its output is the only way to see why. Quiet on
# success, and the tail on failure rather than the whole log.
BUILD_LOG="$(mktemp -t nexus-modeling-build.XXXXXX.log)"
trap 'rm -f "$BUILD_LOG"' EXIT
if ! cmake --build build -j"$(nproc)" >"$BUILD_LOG" 2>&1; then
  tail -60 "$BUILD_LOG"
  echo "BUILD FAILED"
  exit 1
fi

# --output-on-failure so a red run says which test and why. Under `set -e` a
# non-zero exit from ctest ends the script here, which is the whole gate.
ctest --test-dir build --output-on-failure

echo "PASS"
