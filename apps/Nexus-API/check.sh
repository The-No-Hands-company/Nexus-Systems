#!/bin/bash
# Nexus API — typecheck plus unit suite. Runs from any cwd.
set -euo pipefail
cd "$(dirname "$0")"
echo -n "nexus-api... "
./node_modules/.bin/tsc --noEmit
./node_modules/.bin/vitest run 2>&1 | tail -5
echo "PASS"
