#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-dashboard..."

# The frontend comes first, and it is built rather than only typechecked.
#
# tests/server.test.ts asserts on the response for "/" — the SPA shell, and the
# Content-Security-Policy stamped on it. That response is served out of
# frontend/dist, which is a build artifact and is not committed, so on a clean
# checkout the backend suite fails on a header it never got the chance to send.
# Locally it passed only because a stale dist happened to be lying around.
#
# `bun run build` also generates the shared design tokens the frontend imports;
# they are generated, not committed, for the same reason.
(
  cd frontend
  bun run check
  bun run build
  bun run test
)

# Scoped to tests/ on purpose. A bare `bun test` walks into frontend/, whose
# suite is vitest with a jsdom environment — bun's runner cannot provide
# `document`, so all of those tests fail on a runner that was never meant to
# execute them. That is the same fault that kept CI red for five days on
# Nexus-Draw; this app simply had no gate for it to show up in.
bun test tests/

echo "PASS"
