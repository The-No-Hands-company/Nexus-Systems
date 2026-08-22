#!/usr/bin/env bash
# Nexus Systems — Production Launcher
# Starts proxy + all apps on nexussystems.vexr.dev
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPLOY="$ROOT/deploy/production"
LOG_DIR="/tmp/nexus-production"
mkdir -p "$LOG_DIR"

export DOMAIN="${DOMAIN:-nexussystems.vexr.dev}"
export PROXY_PORT="${PROXY_PORT:-8080}"

G="\033[32m" Y="\033[33m" R="\033[0m"
log() { echo -e "${G}[nexus]${R} $*"; }
warn(){ echo -e "${Y}[nexus]${R} $*"; }

# ── Cleanup ──
cleanup() {
  log "Shutting down..."
  jobs -p | xargs -r kill 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT INT TERM

# ── Nexus Cloud (port 8787) ──
log "Starting Nexus-Cloud on :8787..."
cd "$ROOT/apps/Nexus-Cloud"
NEXUS_CLOUD_API_KEY="${NEXUS_CLOUD_API_KEY:-change-me}" \
PORT=8787 \
CORS_ORIGIN="*" \
bun run src/index.ts > "$LOG_DIR/cloud.log" 2>&1 &
CLOUD_PID=$!

for i in $(seq 1 15); do
  if curl -s http://localhost:8787/health > /dev/null 2>&1; then
    log "Nexus-Cloud ready (PID $CLOUD_PID)"
    break
  fi
  [ $i -eq 15 ] && warn "Nexus-Cloud may not be ready"
  sleep 1
done

# ── Nexus Team Chat (port 3109) ──
log "Starting Nexus-Team-Chat on :3109..."
cd "$ROOT/apps/Nexus-Team-Chat"
PORT=3109 \
bun run src/index.ts > "$LOG_DIR/chat.log" 2>&1 &
CHAT_PID=$!

for i in $(seq 1 10); do
  if curl -s http://localhost:3109 > /dev/null 2>&1; then
    log "Nexus-Team-Chat ready (PID $CHAT_PID)"
    break
  fi
  [ $i -eq 10 ] && warn "Nexus-Team-Chat may not be ready"
  sleep 1
done

# ── Reverse Proxy (port 80) ──
log "Starting reverse proxy on :$PROXY_PORT..."
cd "$DEPLOY"
PROXY_PORT="$PROXY_PORT" DOMAIN="$DOMAIN" DASHBOARD_UPSTREAM=http://127.0.0.1:3132 \
bun run proxy.ts > "$LOG_DIR/proxy.log" 2>&1 &
PROXY_PID=$!
sleep 1
log "Proxy ready (PID $PROXY_PID)"

# ── Summary ──
echo ""
log "══════════════════════════════════════════"
log "  Nexus Systems — Production"
log "  Domain: $DOMAIN"
log ""
log "  Proxy:  http://$DOMAIN:$PROXY_PORT"
log "  Cloud:  http://cloud.$DOMAIN:$PROXY_PORT"
log "  Chat:   http://chat.$DOMAIN:$PROXY_PORT"
log ""
log "  PIDs: Proxy=$PROXY_PID Cloud=$CLOUD_PID Chat=$CHAT_PID"
log "  Logs: $LOG_DIR/"
log "══════════════════════════════════════════"
echo ""

wait
