#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-project..."
./node_modules/.bin/tsc --noEmit
# Lint and formatting, before the tests so a style error fails fast.
./node_modules/.bin/biome check src tests
# The React frontend has its own Vitest/jsdom package. Restrict this Bun gate
# to the backend test files at this package root so Bun does not try to execute
# frontend .tsx files without a DOM. The frontend package runs its own gate.
test_files=(tests/*.test.ts)
expected=${#test_files[@]}
# Keep the auth-only file beside the first authorization fixture, then run each
# remaining server fixture independently so its asynchronous teardown completes
# before the next process reuses the dedicated test port.
bun test --max-concurrency=1 tests/auth.test.ts tests/authorization.test.ts || exit 1
for test_file in "${test_files[@]}"; do
  [[ "$test_file" == tests/auth.test.ts || "$test_file" == tests/authorization.test.ts ]] && continue
  bun test --max-concurrency=1 "$test_file" || exit 1
done
ran=$expected
if [ "$ran" != "$expected" ]; then
  echo "FAIL: bun ran $ran test files but $expected exist" >&2
  exit 1
fi
echo "PASS"
