#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-identity..."
# Every test here is a rejection path. The header this verifies arrives on an
# ordinary HTTP request, so each one is the only thing standing between a real
# user and someone typing a header by hand.
bun test tests/
echo "PASS"
