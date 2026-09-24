#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "$(basename "$PWD")..."

# Scoped to tests/ on purpose. A bare `bun test` walks into frontend/, whose
# suite is vitest with a jsdom environment and React — bun's runner cannot load
# it, and for five days CI failed here on `react/jsx-dev-runtime` because the
# workflow ran the bare form instead of this file.
bun test tests/

# The frontend is a separate package with its own toolchain, so it gets its own
# typecheck and its own runner rather than being swept up by the line above.
(
  cd frontend
  bun run check
  bun run test
)

# Output is NOT piped through `tail -1`. It used to be, which made a failing
# frontend test exit 2 with no test name, no assertion and no stack — the gate
# went red and said nothing. A gate that cannot tell you why costs more than it
# saves, and vitest's summary is the last thing worth keeping when it fails.
echo "PASS"
