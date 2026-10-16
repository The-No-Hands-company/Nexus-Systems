#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Install deps if needed
if [ ! -d node_modules ]; then
  echo "[nit-ci] installing dependencies..."
  bun install
fi

# Install Playwright browsers if needed
if ! bunx playwright --version >/dev/null 2>&1 || [ ! -d "$HOME/.cache/ms-playwright" ]; then
  echo "[nit-ci] installing Playwright browsers..."
  bunx playwright install chromium --with-deps
fi

echo "[nit-ci] running..."
exec bun run src/index.ts "$@"
