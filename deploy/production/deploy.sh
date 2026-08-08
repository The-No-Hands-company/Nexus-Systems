#!/usr/bin/env bash
# Nexus Systems — Production Deployer for tnhc.dev (this machine)
# Uses nohup for proper background process management

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LOG_DIR="/tmp/nexus-production"
PID_DIR="$LOG_DIR/pids"
mkdir -p "$LOG_DIR" "$PID_DIR"

export DOMAIN="${DOMAIN:-tnhc.dev}"
export CLOUD_PORT=8787
export CHAT_PORT=3109
export PROXY_PORT=8080
export CLOUD_URL="http://localhost:8787"

G="\033[32m" Y="\033[33m" R="\033[0m"
log()  { echo -e "${G}[nexus]${R} $*"; }
warn() { echo -e "${Y}[nexus]${R} $*"; }

check_port() {
    local port=$1
    if ss -ltn | grep -q ":$port "; then
        warn "Port $port already in use"
        return 1
    fi
    return 0
}

start_service() {
    local name=$1
    local dir=$2
    local port=$3
    shift 3

    check_port $port || return 1

    log "Starting $name on :$port"
    cd "$dir"
    # Use env to set environment variables properly
    nohup env "$@" > "$LOG_DIR/$name.log" 2>&1 &
    local pid=$!
    echo $pid > "$PID_DIR/$name.pid"
    sleep 2

    if kill -0 $pid 2>/dev/null; then
        log "$name started (PID: $pid)"
        return 0
    else
        warn "$name failed to start - check $LOG_DIR/$name.log"
        return 1
    fi
}

cmd_start() {
    : "${NEXUS_CLOUD_API_KEY:?must be set — export it or source your secrets file before deploying}"

    log "Starting Nexus Systems on $DOMAIN..."

    # 1. Infrastructure
    log "Starting infrastructure (Postgres, Redis, MinIO)..."
    cd "$ROOT"
    docker-compose up -d
    sleep 5

    # 2. Nexus Cloud
    #
    # NEXUS_CLOUD_DOMAIN must track $DOMAIN. It is the base Cloud mints public
    # subdomains under, the zone /api/v1/routes is keyed by, and the only suffix
    # /api/v1/routes/tls-ask will authorise a certificate for. Left unset it
    # defaults to "nexus.local", so Cloud would publish *.nexus.local routes
    # that the proxy — serving $DOMAIN — rejects as foreign hosts.
    #
    # CF_* and SERVER_PUBLIC_IP are optional: with a Zone:DNS:Edit token Cloud
    # creates the DNS records for new subdomains itself. Passed through only
    # when present in the environment.
    start_service "cloud" "$ROOT/apps/Nexus-Cloud" 8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        NEXUS_CLOUD_DOMAIN="$DOMAIN" \
        CF_API_TOKEN="${CF_API_TOKEN:-}" \
        CF_ZONE_ID="${CF_ZONE_ID:-}" \
        SERVER_PUBLIC_IP="${SERVER_PUBLIC_IP:-}" \
        PORT=8787 CORS_ORIGIN="*" NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_STORAGE_S3_ENDPOINT=http://localhost:9000 \
        NEXUS_STORAGE_S3_ACCESS_KEY=minioadmin \
        NEXUS_STORAGE_S3_SECRET_KEY=minioadmin \
        NEXUS_STORAGE_S3_REGION=us-east-1 \
        NEXUS_STORAGE_S3_BUCKET_PREFIX=nexus \
        bun run src/index.ts

    # 3. Nexus Team Chat
    start_service "chat" "$ROOT/apps/Nexus-Team-Chat" 3109 \
        NEXUS_CLOUD_URL=http://localhost:8787 PORT=3109 \
        bun run src/index.ts

    # 4. Proxy (8080 for Cloudflare Tunnel)
    start_service "proxy" "$ROOT/deploy/production" 8080 \
        PROXY_PORT=8080 DOMAIN="$DOMAIN" CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        bun run proxy.ts

    # 5. Verify
    sleep 3
    cmd_status
}

cmd_stop() {
    log "Stopping all Nexus services..."
    for svc in cloud chat proxy; do
        if [ -f "$PID_DIR/$svc.pid" ]; then
            kill "$(cat "$PID_DIR/$svc.pid")" 2>/dev/null && log "  Stopped $svc" || true
            rm -f "$PID_DIR/$svc.pid"
        fi
    done
    cd "$ROOT"
    docker-compose down
    log "All stopped"
}

cmd_status() {
    echo "Service Status:"
    for svc in cloud chat proxy; do
        if [ -f "$PID_DIR/$svc.pid" ]; then
            if kill -0 "$(cat "$PID_DIR/$svc.pid")" 2>/dev/null; then
                echo -e "  ${G}● $svc${R} (running, PID: $(cat $PID_DIR/$svc.pid))"
            else
                echo -e "  ${R}✗ $svc${R} (dead)"
                rm -f "$PID_DIR/$svc.pid"
            fi
        else
            echo -e "  ${R}? $svc${R} (not started)"
        fi
    done

    # Check HTTP endpoints
    echo ""
    echo "HTTP Health Checks:"
    for endpoint in "http://localhost:8787/health" "http://localhost:3109/health" "http://localhost:8080/health"; do
        if curl -s -m 2 "$endpoint" | grep -q "ok"; then
            echo -e "  ${G}●${R} $endpoint"
        else
            echo -e "  ${R}✗${R} $endpoint"
        fi
    done
}

case "${1:-}" in
    --bg) cmd_start; echo "Services started. Logs: $LOG_DIR/*.log" ;;
    --stop) cmd_stop ;;
    --status) cmd_status ;;
    *) cmd_start; echo "Press Ctrl+C to stop"; while true; do sleep 1; done ;;
esac
