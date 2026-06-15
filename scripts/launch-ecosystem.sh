#!/bin/bash
# Nexus Ecosystem Launcher — starts the core infrastructure stack
# Usage: ./launch-ecosystem.sh [--all]

set -e
ROOT="/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/apps"
PIDS=()

cleanup() {
  echo -e "\n[nexus] Shutting down ecosystem..."
  for pid in "${PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null
  echo "[nexus] All services stopped."
}

trap cleanup EXIT INT TERM

launch() {
  local name=$1
  local dir="$ROOT/$name"
  local port=$2
  local cmd=${3:-"bun run src/index.ts"}

  echo "[nexus] Starting $name on port $port..."
  cd "$dir"
  $cmd &
  local pid=$!
  PIDS+=($pid)
  sleep 0.5
}

echo "============================================"
echo "  Nexus Ecosystem — Core Infrastructure"
echo "============================================"
echo ""

# ── Tier 1: Core Infrastructure ──
echo "--- Tier 1: Core Infrastructure ---"
launch "Nexus-Cloud" "8787"
launch "Nexus-Guardian" "4320"
launch "Nexus-Auth" "4310"
launch "Nexus-Edge" "4340"
launch "Nexus-Tunnel" "4330"

sleep 1

# ── Tier 2: Platform Essentials ──
echo ""
echo "--- Tier 2: Platform Essentials ---"
launch "Nexus-Files" "3033"
launch "Nexus-Search" "3034"
launch "Nexus-Monitor" "3030"
launch "Nexus-API" "3036"

sleep 1

echo ""
echo "============================================"
echo "  Ecosystem running!"
echo ""
echo "  Cloud:     http://localhost:8787"
echo "  Guardian:  http://localhost:4320"
echo "  Auth:      http://localhost:4310"
echo "  Edge:      http://localhost:4340"
echo "  Tunnel:    http://localhost:4330"
echo "  Files:     http://localhost:3033"
echo "  Search:    http://localhost:3034"
echo "  Monitor:   http://localhost:3030"
echo "  API:       http://localhost:3036"
echo ""
echo "  Press Ctrl+C to stop all services."
echo "============================================"

# Wait for any process to exit
wait
