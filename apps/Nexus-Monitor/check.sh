#!/usr/bin/env bash
# Quality gate for Nexus-Monitor. Runs from any working directory.
set -euo pipefail
cd "$(dirname "$0")"

echo "nexus-monitor..."

# If node_modules lives on a noexec mount, stage executable esbuild copies in /tmp
if [ -z "${ESBUILD_BINARY_PATH:-}" ]; then
  ESBUILD_SRC="frontend/node_modules/@esbuild/linux-x64/bin/esbuild"
  if [ -f "$ESBUILD_SRC" ] && [ ! -x "$ESBUILD_SRC" ]; then
    TMP_TOOLS="${TMPDIR:-/tmp}/nexus-monitor-tools"
    mkdir -p "$TMP_TOOLS"
    cp "$ESBUILD_SRC" "$TMP_TOOLS/esbuild"
    chmod +x "$TMP_TOOLS/esbuild"
    export ESBUILD_BINARY_PATH="$TMP_TOOLS/esbuild"
  fi
fi

bun run typecheck
bun test

if [ -d frontend/node_modules ]; then
  node ./frontend/node_modules/typescript/bin/tsc -p frontend/tsconfig.json --noEmit
fi

echo "PASS"
