#!/bin/bash
# Nexus API — typecheck plus unit suite. Runs from any cwd.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-api..."
bunx tsc -p tsconfig.gateway.json --noEmit
bun test tests/contract-parity.test.ts tests/public-api.test.ts tests/server.test.ts tests/calendar-boundary.test.ts tests/delegation-boundary.test.ts
echo "PASS"
