#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
PROJECT_DIR="$ROOT_DIR/examples/showcase_workflow"
cd "$PROJECT_DIR"

echo "[1/6] lint"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli lint

echo "[2/6] analyze"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli check

echo "[3/6] build"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli build --clean

echo "[4/6] run"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli run

echo "[5/6] profile"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli profile src/main.nxl

echo "[6/6] runtime analyzer + sanitizer"
PYTHONPATH="$ROOT_DIR/src" python -m nexuslang.cli run --analyze-runtime --sanitize address

echo "pipeline complete"
