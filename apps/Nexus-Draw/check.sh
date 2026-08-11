#!/bin/bash
set -euo pipefail
echo -n "$(basename $(dirname $0))... "
bun test tests/ 2>&1 | tail -1
(cd frontend && bun run check >/dev/null && bun run test 2>&1 | tail -1)
echo "PASS"
