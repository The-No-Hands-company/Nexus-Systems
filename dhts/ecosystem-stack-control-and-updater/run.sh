#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOOL_DIR="$ROOT_DIR/dhts/ecosystem-stack-control-and-updater"

python "$TOOL_DIR/stack_control.py" \
  --workspace "$ROOT_DIR" \
  --config "$TOOL_DIR/stack-control.config.json" \
  --json-out "$TOOL_DIR/reports/latest.json" \
  --md-out "$TOOL_DIR/reports/latest.md"
