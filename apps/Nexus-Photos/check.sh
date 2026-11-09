#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "$(basename "$PWD")..."

bun test --max-concurrency=1

if [ -d "frontend" ] && [ -f "frontend/package.json" ]; then
  (cd frontend && bun run build)
fi

echo "PASS"
