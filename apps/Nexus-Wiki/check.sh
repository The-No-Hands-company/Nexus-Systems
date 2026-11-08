#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Nexus-Wiki Quality Gate ==="

echo "1. Python backend tests..."
python3 -m pytest tests/test_api.py -v

echo "2. Frontend build verification..."
cd "$SCRIPT_DIR/frontend"
ESBUILD_BINARY_PATH=/tmp/esbuild bun run build

echo "=== All Nexus-Wiki checks passed! ==="
