#!/usr/bin/env bash
# Nexus Systems – Unified Runner
# Starts Nexus-Cloud + every app with a package.json in apps/
# Usage:
#   ./run-all.sh              start everything
#   ./run-all.sh --stop       stop everything
#   ./run-all.sh --status     show running apps

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APPS="$ROOT/apps"
DATA="$ROOT/data"
PID_DIR="$DATA/pids"
CLOUD_PORT=8787

mkdir -p "$PID_DIR"

# ── helpers ────────────────────────────────────────────────────────

start_one() {
  local name="$1" port="$2" app_dir="$3"
  local env_var="NEXUS_${name//-/_}_ENABLE_CLOUD_INTEGRATION"
  local pid_file="$PID_DIR/$name.pid"
  local log_file="$DATA/logs/$name.log"

  [ -f "$pid_file" ] && kill -0 "$(cat "$pid_file")" 2>/dev/null && return 0

  export PORT="$port"
  export "$env_var=true"
  export NEXUS_CLOUD_URL="http://localhost:$CLOUD_PORT"
  export NEXUS_CLOUD_API_KEY="change-me"

  mkdir -p "$(dirname "$log_file")"
  cd "$app_dir"
  bun run src/index.ts >> "$log_file" 2>&1 &
  echo $! > "$pid_file"
}

stop_one() {
  local name="$1"
  local pid_file="$PID_DIR/$name.pid"
  [ -f "$pid_file" ] && kill "$(cat "$pid_file")" 2>/dev/null; rm -f "$pid_file"
}

status_one() {
  local name="$1"
  local pid_file="$PID_DIR/$name.pid"
  if [ -f "$pid_file" ] && kill -0 "$(cat "$pid_file")" 2>/dev/null; then
    echo "  $name – RUNNING (pid $(cat "$pid_file"))"
  else
    echo "  $name – stopped"
  fi
}

scan_apps() {
  for d in "$APPS"/*/; do
    [ -f "$d/package.json" ] || continue
    basename "$d"
  done | sort
}

# ── commands ───────────────────────────────────────────────────────

cmd_start() {
  echo "=== Nexus Systems Launcher ==="

  echo "Starting Nexus-Cloud on $CLOUD_PORT ..."
  start_one "Nexus-Cloud" "$CLOUD_PORT" "$APPS/Nexus-Cloud"
  sleep 3

  local idx=0
  local ports=()
  for name in $(scan_apps); do
    [ "$name" = "Nexus-Cloud" ] && continue
    [ "$name" = "Phantom" ] && continue    # submodule
    [ "$name" = "Nexus-Network" ] && continue
    [ "$name" = "Nexus-Vault" ] && continue
    [ "$name" = "Nexus-Modeling" ] && continue

    local port=$((3080 + idx))
    [ "$name" = "Nexus-Graphic" ] && port=3080
    [ "$name" = "Nexus-Design" ]  && port=3074
    [ "$name" = "Nexus-Draw" ]    && port=3075
    [ "$name" = "Nexus-Photos" ]  && port=3096
    [ "$name" = "Nexus-Video" ]   && port=3113
    [ "$name" = "Nexus-Cloud" ]   && port=8787

    start_one "$name" "$port" "$APPS/$name"
    ports+=("$name:$port")
    idx=$((idx + 1))
    sleep 0.1  # stagger starts
  done

  echo ""
  echo "Started $((${#ports[@]} + 1)) services."
  echo "  Nexus-Cloud:  http://localhost:$CLOUD_PORT"
  for p in "${ports[@]}"; do
    echo "  ${p%:*}:  http://localhost:${p#*:}"
  done
}

cmd_stop() {
  echo "Stopping all Nexus services ..."
  for name in $(scan_apps); do
    stop_one "$name"
  done
  stop_one "Nexus-Cloud"
  rm -rf "$PID_DIR"
  echo "Done."
}

cmd_status() {
  echo "Nexus Services:"
  status_one "Nexus-Cloud"
  for name in $(scan_apps); do
    [ "$name" = "Nexus-Cloud" ] && continue
    status_one "$name"
  done
}

# ── dispatch ───────────────────────────────────────────────────────

case "${1:-}" in
  --stop)    cmd_stop ;;
  --status)  cmd_status ;;
  *)         cmd_start ;;
esac
