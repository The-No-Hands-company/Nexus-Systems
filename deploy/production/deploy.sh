#!/usr/bin/env bash
# Nexus Systems — Production Deployer for tnhc.dev (this machine)
# Uses nohup for proper background process management

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LOG_DIR="/tmp/nexus-production"
PID_DIR="$LOG_DIR/pids"
mkdir -p "$LOG_DIR" "$PID_DIR"

export DOMAIN="${DOMAIN:-tnhc.dev}"
# Where a browser is sent to sign in. Exported under the name the apps actually
# read, so every service started here inherits it — start_service adds to the
# environment rather than replacing it, and apps fall back to NEXUS_AUTH_URL
# (an internal address, useless to a browser) when it is unset.
#
# Deliberately auth.$DOMAIN and not the apex: the apex is the marketing site on
# Cloudflare Pages, which never reaches this tunnel. https://$DOMAIN/login
# returns the marketing SPA, so apps pointed at the apex sent people to a page
# with no login form on it — and that was invisible to local testing, where
# curl -H "Host: $DOMAIN" against the proxy renders the real thing.
export NEXUS_AUTH_PUBLIC_URL="${NEXUS_AUTH_PUBLIC_URL:-https://auth.$DOMAIN}"
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
    # Cloud's key normally lives in apps/Nexus-Cloud/.env, which bun auto-loads
    # and which is gitignored. Adopt it when nothing is exported, so there is one
    # source of truth and the check below tests the value Cloud will really use
    # rather than a second copy of it. The stop is hard because an empty key
    # makes Cloud's requiresApiKey() false, disabling auth on every mutating
    # endpoint instead of failing closed.
    if [ -z "${NEXUS_CLOUD_API_KEY:-}" ] && [ -f "$ROOT/apps/Nexus-Cloud/.env" ]; then
        NEXUS_CLOUD_API_KEY="$(sed -n 's/^NEXUS_CLOUD_API_KEY=//p' "$ROOT/apps/Nexus-Cloud/.env" \
            | head -1 | tr -d '\r' | sed -e 's/^"//' -e 's/"$//' -e "s/^'//" -e "s/'\$//")"
        export NEXUS_CLOUD_API_KEY
        [ -n "$NEXUS_CLOUD_API_KEY" ] && log "Adopted NEXUS_CLOUD_API_KEY from apps/Nexus-Cloud/.env"
    fi
    : "${NEXUS_CLOUD_API_KEY:?not exported and not found in apps/Nexus-Cloud/.env — an empty key disables Cloud auth entirely}"

    log "Starting Nexus Systems on $DOMAIN..."

    # 1. Infrastructure
    log "Starting infrastructure (Postgres, Redis, MinIO)..."
    cd "$ROOT"
    docker-compose up -d
    sleep 5

    # 2. Nexus Auth — the ecosystem's identity service.
    #
    # Started before Cloud, which delegates to it: Cloud, Deploy and Vault all
    # verify sessions here and none of them holds accounts any more, so nobody
    # can sign in to anything if this is down.
    #
    # NEXUS_AUTH_COOKIE_DOMAIN must carry the leading dot. It is what scopes the
    # session cookie to the parent domain so one login reaches every subdomain —
    # without it the cookie is host-only and users are asked to sign in again on
    # each app, which is the whole problem this replaced.
    #
    # NEXUS_AUTH_BASE_URL is the address Cloud should route to, not the address a
    # browser visits — it is the only thing Auth uses it for, and it becomes
    # `upstream` in Cloud's routing table. A public https:// value here is a trap:
    # the proxy takes the hostname from `upstream`, so publishing
    # https://auth.$DOMAIN would make the proxy answer auth.$DOMAIN by fetching
    # auth.$DOMAIN — back out through Cloudflare, into the tunnel, into itself.
    # The browser-facing host is NEXUS_AUTH_PUBLIC_URL, exported above.
    #
    # NEXUS_AUTH_FOUNDER_PASSWORD / _OPERATOR_PASSWORD are deliberately NOT passed
    # here. start_service cd's into the app directory first and bun auto-loads
    # apps/Nexus-Auth/.env from there, so they arrive without ever appearing in
    # argv — anything passed through `env` below is visible to any local user in
    # `ps`. Without them the seed falls back to the "nexus-founder-2026" literal
    # in users.ts, which is published source, on a host reachable from the
    # internet. Note the seed only creates accounts that do not exist; setting
    # these does nothing to an existing store, which has to be rotated through
    # POST /api/v1/auth/users/:id/password.
    start_service "auth" "$ROOT/apps/Nexus-Auth" 4310 \
        PORT=4310 \
        NEXUS_AUTH_BASE_URL="http://127.0.0.1:4310" \
        NEXUS_AUTH_COOKIE_DOMAIN=".$DOMAIN" \
        NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        bun run src/index.ts

    # 3. Nexus Cloud
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
        NEXUS_AUTH_URL=http://localhost:4310 \
        CF_API_TOKEN="${CF_API_TOKEN:-}" \
        CF_ZONE_ID="${CF_ZONE_ID:-}" \
        SERVER_PUBLIC_IP="${SERVER_PUBLIC_IP:-}" \
        PORT=8787 CORS_ORIGIN="*" NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_STORAGE_S3_ENDPOINT=http://localhost:9000 \
        NEXUS_STORAGE_S3_REGION=us-east-1 \
        NEXUS_STORAGE_S3_BUCKET_PREFIX=nexus \
        bun run src/index.ts

    # 4. Nexus Team Chat
    #
    # NEXUS_TEAM_CHAT_BASE_URL must be set here even though it looks redundant.
    # It is what Chat registers with Cloud as its upstream, and leaving it unset
    # does not fall back to the port below — bun auto-loads
    # apps/Nexus-Team-Chat/.env from the app directory, and that file still
    # carries https://chat.nexussystems.vexr.dev from the previous domain. Chat
    # therefore published a dead public hostname as its own upstream, and the
    # proxy hung trying to resolve it. Same failure as NEXUS_AUTH_BASE_URL: an
    # upstream is an address this machine can reach, never a public URL.
    start_service "chat" "$ROOT/apps/Nexus-Team-Chat" 3109 \
        NEXUS_CLOUD_URL=http://localhost:8787 PORT=3109 \
        NEXUS_TEAM_CHAT_BASE_URL=http://127.0.0.1:3109 \
        bun run src/index.ts

    # 5. Proxy (8080 for Cloudflare Tunnel)
    start_service "proxy" "$ROOT/deploy/production" 8080 \
        PROXY_PORT=8080 DOMAIN="$DOMAIN" CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        bun run proxy.ts

    # 6. Verify
    sleep 3
    cmd_status
}

cmd_stop() {
    log "Stopping all Nexus services..."
    for svc in auth cloud chat proxy; do
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
    for svc in auth cloud chat proxy; do
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
    for endpoint in "http://localhost:4310/health" "http://localhost:8787/health" "http://localhost:3109/health" "http://localhost:8080/health"; do
        if curl -s -m 2 "$endpoint" | grep -q "ok"; then
            echo -e "  ${G}●${R} $endpoint"
        else
            echo -e "  ${R}✗${R} $endpoint"
        fi
    done
}

usage() {
    cat <<USAGE
Usage: $(basename "$0") [command]

  (no command)      start in the foreground, Ctrl+C to stop
  bg, --bg          start in the background and return
  stop, --stop      stop all services and the infrastructure containers
  status, --status  report what is running

Environment: DOMAIN (default tnhc.dev), NEXUS_AUTH_PUBLIC_URL, NEXUS_CLOUD_API_KEY
USAGE
}

# Unrecognised arguments must not fall through to cmd_start. They used to: only
# the ---prefixed spellings were matched, so `deploy.sh status` — the spelling
# anyone tries first — deployed production and then sat in the foreground loop
# below, and any typo did the same. An unknown argument now fails without
# touching anything, and the bare words are accepted alongside the flags.
case "${1:-}" in
    "")             cmd_start; echo "Press Ctrl+C to stop"; while true; do sleep 1; done ;;
    bg|--bg)        cmd_start; echo "Services started. Logs: $LOG_DIR/*.log" ;;
    stop|--stop)    cmd_stop ;;
    status|--status) cmd_status ;;
    -h|--help|help) usage ;;
    *)              echo "Unknown command: $1" >&2; usage >&2; exit 2 ;;
esac
