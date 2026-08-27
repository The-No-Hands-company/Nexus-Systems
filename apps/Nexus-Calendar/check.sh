#!/bin/bash
set -euo pipefail
echo -n "nexus-calendar... "
bun run check
bun test
(
  cd frontend
  bun run check
  bun run test
  bun run build
)
echo "PASS"
