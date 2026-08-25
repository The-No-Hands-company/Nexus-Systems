#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "$(basename "$PWD")..."
# Output is NOT piped through `tail -1`. It used to be, which made a failure
# report the run's summary line and nothing else — no test name, no assertion,
# no stack. A gate that goes red without saying why costs more than it saves.
bun test
echo "PASS"
