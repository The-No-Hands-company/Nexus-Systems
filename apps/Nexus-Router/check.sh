#!/bin/bash
# Nexus Router Quality Gate
set -e
cd "$(dirname "$0")"

echo "=== [Nexus-Router] Running Typecheck ==="
bun run typecheck

echo "=== [Nexus-Router] Running Lint ==="
bun run lint

echo "=== [Nexus-Router] Running Tests ==="
bun run test

echo "=== [Nexus-Router] check.sh Passed ==="
