#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-dashboard..."

# Scoped to tests/ on purpose. A bare `bun test` walks into frontend/, whose
# suite is vitest with a jsdom environment — bun's runner cannot provide
# `document`, so all 154 of those tests fail on a runner that was never meant
# to execute them. That is the same fault that kept CI red for five days on
# Nexus-Draw; this app simply had no gate for it to show up in.
bun test tests/

# The frontend is a separate package with its own toolchain, so it gets its own
# typecheck and its own runner.
(
  cd frontend
  bun run check
  bun run test
)

echo "PASS"
