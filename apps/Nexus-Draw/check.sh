#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
export ESBUILD_BINARY_PATH="${ESBUILD_BINARY_PATH:-/tmp/esbuild}"
cd "$(dirname "$0")"
echo "$(basename "$PWD")..."

bun test --max-concurrency=1 tests/

(
  cd frontend
  export ESBUILD_BINARY_PATH="${ESBUILD_BINARY_PATH:-/tmp/esbuild}"
  bun run check
  bun run test
)

echo "PASS"

# Output is NOT piped through `tail -1`. It used to be, which made a failing
# frontend test exit 2 with no test name, no assertion and no stack — the gate
# went red and said nothing. A gate that cannot tell you why costs more than it
# saves, and vitest's summary is the last thing worth keeping when it fails.
echo "PASS"
