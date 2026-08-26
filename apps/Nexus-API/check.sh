#!/bin/bash
# Nexus API — typecheck plus unit suite. Runs from any cwd.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-api..."
./node_modules/.bin/tsc --noEmit
# Not piped through `tail -5`. vitest reports the failing file, test and
# assertion above its summary, and truncating to the last few lines threw all
# of that away on exactly the runs where it mattered.
./node_modules/.bin/vitest run
echo "PASS"
