#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Nexus-Docs Quality Gate ==="

echo "1. Server & Engine tests..."
bun test tests/server.test.ts

echo "2. Frontend build verification..."
cd "$SCRIPT_DIR/frontend"
bun node_modules/typescript/bin/tsc
ESBUILD_BINARY_PATH=/tmp/esbuild bun node_modules/vite/bin/vite.js build

echo "=== All Nexus-Docs checks passed! ==="
