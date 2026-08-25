#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-tunnel..."
# Not piped through `tail -1`. That printed the run's summary line and threw
# away every failure above it, so a red gate named no test and showed no
# assertion. bun's own exit code is the gate; `set -e` acts on it.
bun test
echo "PASS"
