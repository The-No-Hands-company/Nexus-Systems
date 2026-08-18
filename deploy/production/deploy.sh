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

    # An already-bound port means the service is already up, which is a
    # success for our purposes, not a failure. Returning non-zero here aborted
    # the whole script under `set -e`, so a single running service stopped
    # every later one from starting — `deploy.sh bg` could not fill in the
    # gaps after a partial outage, which is precisely when it is needed.
    if ! check_port $port; then
        log "$name already running on :$port — leaving it alone"
        return 0
    fi

    log "Starting $name on :$port"
    cd "$dir"
    # setsid, not just nohup. nohup only makes the process ignore SIGHUP — it
    # leaves it in the launching shell's session, so when that session is torn
    # down (an ssh disconnect, a terminal closing, an agent's shell exiting)
    # the whole group goes with it. auth, cloud and chat all died together
    # twice this way, and the symptom is a 502 at the edge because the tunnel
    # is fine and the origin is simply gone.
    #
    # setsid makes each service its own session leader, so it outlives whatever
    # started it. Job control is off in a non-interactive script, so the child
    # is not already a process-group leader and setsid execs in place rather
    # than forking — which keeps $! pointing at the real process.
    # `env` is retained so the caller's KEY=value arguments still apply.
    setsid nohup env "$@" > "$LOG_DIR/$name.log" 2>&1 &
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

    # Same pattern for Email's database URL: the value lives beside the app so
    # a credential is never written into this script or the repo.
    if [ -z "${NEXUS_EMAIL_DATABASE_URL:-}" ] && [ -f "$ROOT/apps/Nexus-Email/.env" ]; then
        NEXUS_EMAIL_DATABASE_URL="$(sed -n 's/^NEXUS_EMAIL_DATABASE_URL=//p' "$ROOT/apps/Nexus-Email/.env" \
            | head -1 | tr -d '\r"')"
        [ -n "$NEXUS_EMAIL_DATABASE_URL" ] && log "Adopted NEXUS_EMAIL_DATABASE_URL from apps/Nexus-Email/.env"
    fi

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
    # CF_API_TOKEN / CF_ZONE_ID normally come from apps/Nexus-Cloud/.env, which
    # bun auto-loads (start_service cd's into the app dir); they are passed here
    # only if also exported. With a Zone:DNS:Edit token, Cloud publishes DNS for
    # custom domains itself — as proxied CNAMEs to the tunnel, NOT A records: this
    # node has no routable public IP. NEXUS_TUNNEL_ID is the tunnel every hostname
    # is CNAMEd to; it is this node's "Nexus Systems" tunnel and is not secret (it
    # is visible in every cfargotunnel DNS record). tnhc.dev subdomains need no
    # per-name record — the *.tnhc.dev wildcard already covers them — so this only
    # matters for out-of-wildcard custom domains.
    start_service "cloud" "$ROOT/apps/Nexus-Cloud" 8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        NEXUS_CLOUD_DOMAIN="$DOMAIN" \
        NEXUS_AUTH_URL=http://localhost:4310 \
        CF_API_TOKEN="${CF_API_TOKEN:-}" \
        CF_ZONE_ID="${CF_ZONE_ID:-}" \
        NEXUS_TUNNEL_ID="${NEXUS_TUNNEL_ID:-a3fc7587-49de-4792-b532-882775db6457}" \
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
    # PHANTOM_REQUIRE_REAL: verified against the native library, so this one is
    # allowed to insist rather than fall back to counterfeit crypto.
    start_service "chat" "$ROOT/apps/Nexus-Team-Chat" 3109 \
        NEXUS_CLOUD_URL=http://localhost:8787 PORT=3109 \
        PHANTOM_REQUIRE_REAL=1 \
        NEXUS_TEAM_CHAT_BASE_URL=http://127.0.0.1:3109 \
        bun run src/index.ts

    # 4b. nexus-chat (apps/Nexus) — what chat.$DOMAIN now serves.
    #
    # Three ports rather than one: REST 8180, WebSocket gateway 8181, voice
    # 8182. Not nexus-chat's 808x defaults, which would collide with the proxy
    # on 8080 and Hosting's site-proxy on 8090. Caddy below joins all three plus
    # the built SPA into the single origin the proxy can route to, because the
    # SPA's production build calls /api and /gateway same-origin and the proxy
    # maps a hostname to exactly one upstream.
    #
    # Secrets live in deploy/production/nexus-chat.env (gitignored; see
    # nexus-chat.env.example). Skipped rather than fatal when absent, so a node
    # that has not been given the credentials still starts everything else.
    if [ -f "$ROOT/deploy/production/nexus-chat.env" ]; then
        if [ ! -x "$ROOT/apps/Nexus/target/debug/nexus" ]; then
            warn "nexus-chat binary missing — build it with: (cd apps/Nexus && cargo build --bin nexus)"
        else
            # Sourced inside a subshell so it cannot leak. `set -a` exports
            # every variable in that file into this shell, and every service
            # started afterwards inherits them — nexus-chat.env sets
            # PUBLIC_URL=https://chat.tnhc.dev and NEXUS__SERVER__NAME, which
            # is how the dashboard ended up announcing itself to Cloud as
            # chat.tnhc.dev and appearing in its own app grid pointing at Chat.
            (
                set -a; . "$ROOT/deploy/production/nexus-chat.env"; set +a
                start_service "nexus-chat" "$ROOT/apps/Nexus" 8180 \
                    ./target/debug/nexus serve --port 8180 --gateway-port 8181 --voice-port 8182
            )

            # Front door: SPA + /api + /gateway + /voice/ws on one origin.
            # Plain HTTP on 8095 — Cloudflare terminates TLS at the edge and
            # nothing here may hold 80/443.
            if command -v caddy >/dev/null 2>&1; then
                start_service "nexus-chat-web" "$ROOT" 8095 \
                    caddy run --config "$ROOT/deploy/production/nexus-chat.Caddyfile" --adapter caddyfile
            else
                warn "caddy not installed — chat.$DOMAIN has no front door"
            fi
        fi
    else
        warn "deploy/production/nexus-chat.env absent — skipping nexus-chat"
    fi

    # 4d. Nexus-Dashboard — app.$DOMAIN, the ecosystem front door.
    #
    # PUBLIC, deliberately: it carries request-access and claim, which people
    # who are not signed in must be able to reach. The apps behind it are
    # gated; this is the door to them. Gating this host would deadlock exactly
    # as gating auth.$DOMAIN would.
    #
    # It also reverse-proxies /api/v1/auth/* to Auth so the browser never makes
    # a credentialed cross-origin call — hence NEXUS_AUTH_INTERNAL_URL being an
    # address this machine can reach and never a public URL, the same trap
    # NEXUS_AUTH_BASE_URL documents above.
    #
    # Skipped with a warning rather than fatally when the UI has not been
    # built, so a checkout that has not run `npm run build` still brings up
    # everything else.
    if [ ! -f "$ROOT/apps/Nexus-Dashboard/frontend/dist/index.html" ]; then
        warn "dashboard UI not built — run: (cd apps/Nexus-Dashboard/frontend && npm install && npm run build)"
    elif [ ! -d "$ROOT/apps/Nexus-Dashboard/src" ]; then
        warn "apps/Nexus-Dashboard/src missing — skipping dashboard"
    else
        start_service "dashboard" "$ROOT/apps/Nexus-Dashboard" 3132 \
            PORT=3132 DOMAIN="$DOMAIN" \
            NEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310 \
            NEXUS_CLOUD_URL=http://localhost:8787 \
            NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
            bun run src/index.ts
    fi

    # 4e. Nexus-Draw backend — the board/collab API behind draw.$DOMAIN.
    #
    # The SPA at draw.$DOMAIN is a static site served by Hosting and works
    # standalone against localStorage, which is why nobody noticed this was
    # down. Two things need it running: the server-backed board API the
    # frontend now calls, and the Cloud heartbeat — without a heartbeat Cloud
    # marks the tool offline and the dashboard grid renders Draw as
    # "Unavailable" while the site is plainly working.
    # Phantom's native library, which Draw now requires. The build artifact is
    # gitignored, so a fresh checkout has none — and since Draw fails closed on
    # mock cryptography, skipping this would leave it refusing to start with a
    # message about crypto rather than about a missing build. Cheap when
    # already built: cargo no-ops.
    if [ ! -f "$ROOT/packages/phantom-sdk/wasm/target/release/libphantom_wasm.so" ]; then
        log "Building Phantom native library (first run)..."
    fi
    if ! (cd "$ROOT/packages/phantom-sdk/wasm" && cargo build --release >/dev/null 2>&1); then
        warn "Phantom native library failed to build — services requiring real crypto will refuse to start"
    fi

    # PHANTOM_REQUIRE_REAL: refuse to start on mock cryptography. Verified
    # against the native library. Do not add this to a service until its crypto
    # has been confirmed working — it fails closed, which is the point.
    #
    # Only services that actually call the Phantom SDK carry this. Setting it on
    # auth, cloud or the dashboard would do nothing: they never construct an
    # SDK, so there is no fallback for the flag to refuse.
    start_service "draw" "$ROOT/apps/Nexus-Draw" 3075 \
        PORT=3075 \
        PHANTOM_REQUIRE_REAL=1 \
        NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        bun run src/index.ts

    # 4f. Nexus-Email API — the mail store behind app.$DOMAIN/mail.
    #
    # Loopback only, and that is a security control rather than a default: this
    # service trusts the X-Nexus-Subject header the dashboard sets after asking
    # Auth who the caller is, so anything able to reach the port could claim to
    # be anyone. It must never be given a public route.
    if [ -x "$ROOT/apps/Nexus-Email/target/release/nexus-mailapi" ]; then
        MAILAPI_BIN="$ROOT/apps/Nexus-Email/target/release/nexus-mailapi"
    else
        MAILAPI_BIN="$ROOT/apps/Nexus-Email/target/debug/nexus-mailapi"
    fi
    if [ -x "$MAILAPI_BIN" ]; then
        start_service "mailapi" "$ROOT/apps/Nexus-Email" 3140 \
            NEXUS_EMAIL_BIND=127.0.0.1:3140 \
            NEXUS_EMAIL_DOMAIN="$DOMAIN" \
            NEXUS_EMAIL_DATABASE_URL="$NEXUS_EMAIL_DATABASE_URL" \
            "$MAILAPI_BIN"
    else
        log "mailapi binary not built - skipping (cargo build -p nexus-mailapi)"
    fi

    # 4g. Nexus-Email SMTP daemon.
    #
    # Two listeners: an MX port for anonymous strangers, which may only deliver
    # to mailboxes this node hosts, and a submission port for our own users,
    # which requires authentication. The relay guard between them is what keeps
    # this from becoming an open relay, and it is tested both as a state machine
    # and over a real socket.
    #
    # Unprivileged ports by default. Binding 25 needs root or
    # CAP_NET_BIND_SERVICE, and nothing can reach this node on 25 anyway — the
    # ISP filters it and the Cloudflare tunnel does not carry SMTP. Publishing
    # an MX therefore needs both a route in and the capability; until then this
    # serves local and federated submission.
    #
    # NEXUS_EMAIL_POLICY defaults to observe: SPF/DKIM/DMARC are evaluated and
    # recorded in Authentication-Results, but no mail is refused on their
    # account. Switch to enforce after reading those headers against real
    # traffic — not on the day it first runs.
    if [ -x "$ROOT/apps/Nexus-Email/target/release/nexus-mailsmtpd" ]; then
        SMTPD_BIN="$ROOT/apps/Nexus-Email/target/release/nexus-mailsmtpd"
    else
        SMTPD_BIN="$ROOT/apps/Nexus-Email/target/debug/nexus-mailsmtpd"
    fi
    if [ -x "$SMTPD_BIN" ]; then
        start_service "mailsmtpd" "$ROOT/apps/Nexus-Email" "${NEXUS_EMAIL_MX_PORT:-2525}" \
            NEXUS_EMAIL_MX_BIND="${NEXUS_EMAIL_MX_BIND:-127.0.0.1:2525}" \
            NEXUS_EMAIL_SUBMISSION_BIND="${NEXUS_EMAIL_SUBMISSION_BIND:-127.0.0.1:2587}" \
            NEXUS_EMAIL_DOMAIN="$DOMAIN" \
            NEXUS_EMAIL_HOSTNAME="mail.$DOMAIN" \
            NEXUS_EMAIL_POLICY="${NEXUS_EMAIL_POLICY:-observe}" \
            NEXUS_EMAIL_DATABASE_URL="$NEXUS_EMAIL_DATABASE_URL" \
            "$SMTPD_BIN"
    else
        log "mailsmtpd binary not built - skipping (cargo build -p nexus-mailsmtp)"
    fi

    # 5. Proxy (8080 for Cloudflare Tunnel)
    #
    # HOSTING_SITE_UPSTREAM is the default backend for the *.$DOMAIN wildcard:
    # any on-domain host that is not a registered app route nor one of the static
    # app fallbacks is handed to Nexus-Hosting's site-proxy (8090), which serves
    # the deployed site or its own 404. This is what makes deployed sites reachable
    # through the tunnel's wildcard entry while apps keep precedence. Set it empty
    # to 404 unmatched hosts instead (a node not running the Hosting site-proxy).
    start_service "proxy" "$ROOT/deploy/production" 8080 \
        PROXY_PORT=8080 DOMAIN="$DOMAIN" CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        HOSTING_SITE_UPSTREAM="${HOSTING_SITE_UPSTREAM:-http://127.0.0.1:8090}" \
        bun run proxy.ts

    # 6. Verify
    sleep 3
    cmd_status
}

cmd_stop() {
    log "Stopping all Nexus services..."
    for svc in auth cloud chat nexus-chat nexus-chat-web dashboard draw mailapi mailsmtpd proxy; do
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
    for svc in auth cloud chat nexus-chat nexus-chat-web dashboard draw mailapi mailsmtpd proxy; do
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
    # nexus-chat answers /api/v1/health, not /health, and is probed through its
    # Caddy front door on 8095 — that is the origin chat.$DOMAIN actually
    # reaches, so a healthy API behind a dead front door still reads as down.
    for endpoint in "http://localhost:4310/health" "http://localhost:8787/health" "http://localhost:3109/health" "http://localhost:8095/api/v1/health" "http://localhost:3132/health" "http://localhost:3075/health" "http://localhost:8080/health"; do
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
