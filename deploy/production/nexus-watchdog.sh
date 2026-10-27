#!/usr/bin/env bash
# nexus-watchdog.sh — called by nexus-watchdog.timer every 5 minutes.
#
# deploy.sh bg is idempotent: it skips services that are already managed and
# starts any that have gone dark. Running it on a timer means a crashed service
# recovers within one timer interval without any human intervention.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_SCRIPT="$SCRIPT_DIR/deploy.sh"

if [ ! -x "$DEPLOY_SCRIPT" ]; then
    echo "[watchdog] deploy.sh not found or not executable: $DEPLOY_SCRIPT" >&2
    exit 1
fi

echo "[watchdog] $(date -Iseconds) — checking services"
bash "$DEPLOY_SCRIPT" bg
echo "[watchdog] done"
