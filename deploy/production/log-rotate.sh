#!/usr/bin/env bash
# Keeps native service logs to at most 24 hours: run every 12 hours, keep the
# current file and exactly one previous period. copy-truncate, because the
# services hold their log file open (deploy.sh appends with >>).
set -euo pipefail
LOG_DIR="${LOG_DIR:-/tmp/nexus-production}"
shopt -s nullglob
for f in "$LOG_DIR"/*.log; do
  cp -- "$f" "$f.1"
  : > "$f"
done
