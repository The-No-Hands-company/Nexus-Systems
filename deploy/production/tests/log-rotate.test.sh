#!/usr/bin/env bash
set -euo pipefail
d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
printf 'old\n' > "$d/a.log"; printf 'older\n' > "$d/a.log.1"
LOG_DIR="$d" bash "$(dirname "$0")/../log-rotate.sh"
[ "$(cat "$d/a.log.1")" = "old" ] || { echo "FAIL: previous not kept"; exit 1; }
[ ! -s "$d/a.log" ] || { echo "FAIL: current not truncated"; exit 1; }
! grep -q older "$d"/* || { echo "FAIL: older than one period survived"; exit 1; }
echo PASS
