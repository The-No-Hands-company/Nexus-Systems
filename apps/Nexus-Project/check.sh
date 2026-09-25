#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-project..."
./node_modules/.bin/tsc --noEmit
# Lint and formatting, before the tests so a style error fails fast.
./node_modules/.bin/biome check src tests
# A green total only covers files that imported. Compare the number of test
# files bun actually ran with the number on disk, so a file that fails to load
# cannot hide behind a passing count.
expected=$(find tests -name '*.test.ts' | wc -l | tr -d ' ')
output=$(bun test tests/ 2>&1) || { printf '%s\n' "$output"; exit 1; }
printf '%s\n' "$output" | tail -4
ran=$(printf '%s\n' "$output" | sed -n 's/^Ran [0-9]* tests\{0,1\} across \([0-9]*\) files\{0,1\}.*/\1/p')
if [ "$ran" != "$expected" ]; then
  echo "FAIL: bun ran $ran test files but $expected exist" >&2
  exit 1
fi
echo "PASS"
