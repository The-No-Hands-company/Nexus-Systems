#!/usr/bin/env bash
# Nexus Systems — Production Deployer for tnhc.dev (this machine)
# Uses nohup for proper background process management

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG_DIR="${NEXUS_PRODUCTION_LOG_DIR:-/tmp/nexus-production}"
PID_DIR="${NEXUS_PRODUCTION_PID_DIR:-$LOG_DIR/pids}"
CURL_BIN="${CURL_BIN:-curl}"
KILL_BIN="${KILL_BIN:-kill}"
CADDY_BIN="${CADDY_BIN:-caddy}"
CLOUD_ENV_FILE="${NEXUS_CLOUD_ENV_FILE:-$ROOT/apps/Nexus-Cloud/.env}"
NEXUS_CHAT_ENV_FILE="${NEXUS_CHAT_ENV_FILE:-$ROOT/deploy/production/nexus-chat.env}"
NEXUS_CHAT_BINARY_PATH="${NEXUS_CHAT_BINARY_PATH:-$ROOT/apps/Nexus/target/debug/nexus}"
mkdir -p "$LOG_DIR" "$PID_DIR"

# shellcheck source=processes.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/processes.sh"

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
# (removed) CHAT_PORT=3109 — Nexus-Team-Chat scaffold deleted; nexus-chat owns chat now.
export PROXY_PORT=8080
export CLOUD_URL="http://localhost:8787"

G="\033[32m" Y="\033[33m" R="\033[0m"
log()  { echo -e "${G}[nexus]${R} $*"; }
warn() { echo -e "${Y}[nexus]${R} $*"; }

service_identity() {
    local name=$1

    case "$name" in
        auth)           SERVICE_DIR="$ROOT/apps/Nexus-Auth"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        cloud)          SERVICE_DIR="$ROOT/apps/Nexus-Cloud"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        chat)           SERVICE_DIR="$ROOT/apps/Nexus-Team-Chat"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        nexus-chat)     SERVICE_DIR="$ROOT/apps/Nexus"; SERVICE_EXEC_PATTERN="./target/debug/nexus serve --port 8180" ;;
        nexus-chat-web) SERVICE_DIR="$ROOT"; SERVICE_EXEC_PATTERN="caddy run --config $ROOT/deploy/production/nexus-chat.Caddyfile" ;;
        dashboard)      SERVICE_DIR="$ROOT/apps/Nexus-Dashboard"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        draw)           SERVICE_DIR="$ROOT/apps/Nexus-Draw"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        proxy)          SERVICE_DIR="$ROOT/deploy/production"; SERVICE_EXEC_PATTERN="bun run proxy.ts" ;;
        api)            SERVICE_DIR="$ROOT/apps/Nexus-API"; SERVICE_EXEC_PATTERN="--import tsx src/index.ts" ;;
        calendar)       SERVICE_DIR="$ROOT/apps/Nexus-Calendar"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        nexus-calendar-web) SERVICE_DIR="$ROOT"; SERVICE_EXEC_PATTERN="run --config $ROOT/deploy/production/nexus-calendar.Caddyfile" ;;
        nexus-draw-web) SERVICE_DIR="$ROOT"; SERVICE_EXEC_PATTERN="run --config $ROOT/deploy/production/nexus-draw.Caddyfile" ;;
        terminal)       SERVICE_DIR="$ROOT/apps/Nexus-Terminal"; SERVICE_EXEC_PATTERN="bun run src/index.ts" ;;
        mailapi)        SERVICE_DIR="$ROOT/apps/Nexus-Email"; SERVICE_EXEC_PATTERN="/nexus-mailapi" ;;
        mailsmtpd)      SERVICE_DIR="$ROOT/apps/Nexus-Email"; SERVICE_EXEC_PATTERN="/nexus-mailsmtpd" ;;
        *)              warn "No ownership identity configured for $name"; return 1 ;;
    esac
}

service_port() {
    case "$1" in
        auth) printf '%s\n' 4310 ;;
        cloud) printf '%s\n' 8787 ;;
        chat) printf '%s\n' 3109 ;;
        nexus-chat) printf '%s\n' 8180 ;;
        nexus-chat-web) printf '%s\n' 8095 ;;
        dashboard) printf '%s\n' 3132 ;;
        draw) printf '%s\n' 3075 ;;
        proxy) printf '%s\n' 8080 ;;
        api) printf '%s\n' 3150 ;;
        calendar) printf '%s\n' 3068 ;;
        nexus-calendar-web) printf '%s\n' 8092 ;;
        nexus-draw-web) printf '%s\n' 8091 ;;
        terminal) printf '%s\n' 3110 ;;
        mailapi) printf '%s\n' 3140 ;;
        mailsmtpd) printf '%s\n' "${NEXUS_EMAIL_MX_PORT:-2525}" ;;
        *) return 1 ;;
    esac
}

start_service() {
    local name=$1
    local dir=$2
    local port=$3
    local pid state launched_pid attempt start_attempts start_interval cleanup_attempts cleanup_interval
    shift 3

    if ! service_identity "$name"; then
        return 1
    fi
    if [ "$dir" != "$SERVICE_DIR" ]; then
        warn "$name directory does not match its ownership identity"
        return 1
    fi

    # A bound port is only an already-running service when all independent
    # ownership signals agree. Never adopt an unrelated listener just because
    # it happens to use the expected port.
    if pid="$(reconcile_pid "$name" "$port" "$dir" "$SERVICE_EXEC_PATTERN" 2>/dev/null)"; then
        log "$name already managed on :$port (PID: $pid)"
        return 0
    fi
    state="$(listener_state "$port" 2>/dev/null)"
    case "$state" in
        absent) ;;
        occupied)
            warn "$name has an occupied but unverifiable port :$port — refusing to start"
            return 1
            ;;
        unverifiable)
            warn "$name cannot inspect :$port safely — refusing to start"
            return 1
            ;;
        *)
            warn "$name cannot determine the state of :$port safely — refusing to start"
            return 1
            ;;
    esac

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
    # Append until the checkpoint-gated cleanup command explicitly replaces
    # the affected log. A containment restart must not silently discard the
    # historical evidence (or the credentials that still need invalidation).
    setsid nohup env "$@" >> "$LOG_DIR/$name.log" 2>&1 &
    launched_pid=$!
    start_attempts="${NEXUS_PRODUCTION_START_ATTEMPTS:-20}"
    start_interval="${NEXUS_PRODUCTION_START_INTERVAL:-0.25}"
    for ((attempt = 1; attempt <= start_attempts; attempt++)); do
        if pid="$(reconcile_pid "$name" "$port" "$dir" "$SERVICE_EXEC_PATTERN" 2>/dev/null)"; then
            log "$name started and managed (PID: $pid)"
            return 0
        fi
        if [ "$attempt" -lt "$start_attempts" ]; then
            sleep "$start_interval"
        fi
    done

    # `$!` is not trusted merely because this script launched it. Revalidate
    # cwd and command line before signalling a process that never reached the
    # listener/PID checkpoint. This keeps late binds from blocking rollback.
    if pid_process_identity_matches "$launched_pid" "$dir" "$SERVICE_EXEC_PATTERN" 2>/dev/null; then
        if "$KILL_BIN" "$launched_pid" 2>/dev/null; then
            cleanup_attempts="${NEXUS_PRODUCTION_LAUNCH_CLEANUP_ATTEMPTS:-20}"
            cleanup_interval="${NEXUS_PRODUCTION_LAUNCH_CLEANUP_INTERVAL:-0.1}"
            for ((attempt = 1; attempt <= cleanup_attempts; attempt++)); do
                kill -0 "$launched_pid" 2>/dev/null || break
                sleep "$cleanup_interval"
            done
            if kill -0 "$launched_pid" 2>/dev/null \
                && pid_process_identity_matches "$launched_pid" "$dir" "$SERVICE_EXEC_PATTERN" 2>/dev/null; then
                "$KILL_BIN" -KILL "$launched_pid" 2>/dev/null || true
            fi
            if kill -0 "$launched_pid" 2>/dev/null; then
                warn "$name missed the listener checkpoint and its exact launcher PID is still live"
            else
                warn "$name missed the bounded listener checkpoint; stopped its exact launcher PID $launched_pid"
            fi
        else
            warn "$name missed the bounded listener checkpoint and its exact launcher PID could not be stopped"
        fi
    fi
    warn "$name did not become safely managed (launcher PID: $launched_pid) - check $LOG_DIR/$name.log"
    return 1
}

validate_cloud_environment() {
    local cloud_env_file="${NEXUS_CLOUD_ENV_FILE:-$CLOUD_ENV_FILE}"

    [ -f "$cloud_env_file" ] || {
        warn "protected Nexus Cloud environment is missing; refusing to start Cloud"
        return 1
    }
    (
        unset NEXUS_CLOUD_API_KEY CF_API_TOKEN \
            NEXUS_STORAGE_S3_ACCESS_KEY NEXUS_STORAGE_S3_SECRET_KEY \
            NEXUS__STORAGE__ACCESS_KEY NEXUS__STORAGE__SECRET_KEY
        set -a
        # shellcheck source=/dev/null
        . "$cloud_env_file"
        set +a
        [ -n "${NEXUS_CLOUD_API_KEY:-}" ] \
            && [ -n "${NEXUS_STORAGE_S3_ACCESS_KEY:-}" ] \
            && [ -n "${NEXUS_STORAGE_S3_SECRET_KEY:-}" ]
    ) || {
        warn "protected Nexus Cloud API/storage credentials are incomplete; refusing to start Cloud"
        return 1
    }
}

adopt_cloud_registration_environment_for_dependents() {
    local cloud_env_file="${NEXUS_CLOUD_ENV_FILE:-$CLOUD_ENV_FILE}"
    local -a registration_values=()

    validate_cloud_environment || return 1
    # Source in an isolated shell so duplicate assignments, quoting, and
    # expansions have exactly the same last-assignment semantics as Cloud's
    # protected launcher. Only the two registration values cross the pipe.
    mapfile -d '' -t registration_values < <(
        (
            unset NEXUS_CLOUD_API_KEY NEXUS_CLOUD_URL
            set -a
            # shellcheck source=/dev/null
            . "$cloud_env_file"
            set +a
            [ -n "${NEXUS_CLOUD_API_KEY:-}" ] && [ -n "${NEXUS_CLOUD_URL:-}" ] || exit 1
            printf '%s\0%s\0' "$NEXUS_CLOUD_API_KEY" "$NEXUS_CLOUD_URL"
        ) 2>/dev/null
    )
    if [ "${#registration_values[@]}" -ne 2 ] \
        || [ -z "${registration_values[0]}" ] \
        || [ -z "${registration_values[1]}" ]; then
        warn "protected Cloud registration URL/key are incomplete; refusing to start dependent services"
        return 1
    fi
    NEXUS_CLOUD_API_KEY=${registration_values[0]}
    NEXUS_CLOUD_URL=${registration_values[1]}
    export NEXUS_CLOUD_API_KEY
    export NEXUS_CLOUD_URL
}

validate_nexus_chat_caddy() {
    if ! command -v "$CADDY_BIN" >/dev/null 2>&1; then
        warn "caddy is not installed — chat.$DOMAIN has no front door"
        return 1
    fi
    if ! "$CADDY_BIN" validate \
        --config "$ROOT/deploy/production/nexus-chat.Caddyfile" \
        --adapter caddyfile; then
        warn "Nexus Chat Caddy configuration did not validate; leaving the current service untouched"
        return 1
    fi
}

start_cloud_service() {
    validate_cloud_environment || return 1
    start_service "cloud" "$ROOT/apps/Nexus-Cloud" 8787 \
        -u NEXUS_CLOUD_API_KEY -u CF_API_TOKEN \
        -u NEXUS_STORAGE_S3_ACCESS_KEY -u NEXUS_STORAGE_S3_SECRET_KEY \
        -u NEXUS__STORAGE__ACCESS_KEY -u NEXUS__STORAGE__SECRET_KEY \
        NEXUS_CLOUD_DOMAIN="$DOMAIN" \
        NEXUS_AUTH_URL=http://localhost:4310 \
        CF_ZONE_ID="${CF_ZONE_ID:-}" \
        NEXUS_TUNNEL_ID="${NEXUS_TUNNEL_ID:-a3fc7587-49de-4792-b532-882775db6457}" \
        SERVER_PUBLIC_IP="${SERVER_PUBLIC_IP:-}" \
        PORT=8787 CORS_ORIGIN="*" NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_STORAGE_S3_ENDPOINT=http://localhost:9000 \
        NEXUS_STORAGE_S3_REGION=us-east-1 \
        NEXUS_STORAGE_S3_BUCKET_PREFIX=nexus \
        bash "$ROOT/deploy/production/start-cloud.sh"
}

validate_nexus_chat_environment() {
    (
        adopt_cloud_registration_environment_for_dependents
    ) || {
        warn "protected Cloud registration configuration is incomplete; refusing to restart nexus-chat"
        return 1
    }
    [ -f "$NEXUS_CHAT_ENV_FILE" ] || {
        warn "protected nexus-chat environment is missing; refusing to restart nexus-chat"
        return 1
    }
    [ -x "$NEXUS_CHAT_BINARY_PATH" ] || {
        warn "nexus-chat binary is missing; refusing to restart nexus-chat"
        return 1
    }
    (
        unset NEXUS__STORAGE__ENDPOINT NEXUS__STORAGE__ACCESS_KEY \
            NEXUS__STORAGE__SECRET_KEY NEXUS__STORAGE__BUCKET
        set -a
        # shellcheck source=/dev/null
        . "$NEXUS_CHAT_ENV_FILE"
        set +a
        [ -n "${NEXUS__STORAGE__ENDPOINT:-}" ] \
            && [ -n "${NEXUS__STORAGE__ACCESS_KEY:-}" ] \
            && [ -n "${NEXUS__STORAGE__SECRET_KEY:-}" ] \
            && [ -n "${NEXUS__STORAGE__BUCKET:-}" ]
    ) || {
        warn "protected nexus-chat storage configuration is incomplete; refusing to restart nexus-chat"
        return 1
    }
}

start_nexus_chat_service() {
    validate_nexus_chat_environment || return 1
    (
        unset NEXUS__STORAGE__ENDPOINT NEXUS__STORAGE__ACCESS_KEY \
            NEXUS__STORAGE__SECRET_KEY NEXUS__STORAGE__BUCKET \
            NEXUS_CLOUD_API_KEY NEXUS_CLOUD_URL
        set -a
        # shellcheck source=/dev/null
        . "$NEXUS_CHAT_ENV_FILE"
        set +a
        # The protected Cloud file is authoritative for registration. Load it
        # after Chat so stale copies in either the shell or Chat file cannot
        # override the URL/key used by Nexus's startup registration path.
        adopt_cloud_registration_environment_for_dependents || return 1
        start_service "nexus-chat" "$ROOT/apps/Nexus" 8180 \
            ./target/debug/nexus serve --port 8180 --gateway-port 8181 --voice-port 8182
    )
}

start_nexus_chat_web_service() {
    validate_nexus_chat_caddy || return 1
    start_service "nexus-chat-web" "$ROOT" 8095 \
        "$CADDY_BIN" run --config "$ROOT/deploy/production/nexus-chat.Caddyfile" --adapter caddyfile
}

preflight_service_start() {
    case "$1" in
        cloud) validate_cloud_environment ;;
        nexus-chat) validate_nexus_chat_environment ;;
        nexus-chat-web) validate_nexus_chat_caddy ;;
        *) warn "Per-service recovery is limited to cloud, nexus-chat, and nexus-chat-web"; return 1 ;;
    esac
}

start_named_service() {
    case "$1" in
        cloud) start_cloud_service ;;
        nexus-chat) start_nexus_chat_service ;;
        nexus-chat-web) start_nexus_chat_web_service ;;
        *) warn "Per-service recovery is limited to cloud, nexus-chat, and nexus-chat-web"; return 1 ;;
    esac
}

# Native service logs live at most 24 hours: a user timer rotates $LOG_DIR every
# 12 hours (log-rotate.sh keeps the current file plus one previous period).
# Never fatal - a missing user manager must not stop the stack from starting.
install_log_rotation() {
    [ "${NEXUS_SKIP_LOG_ROTATE_INSTALL:-0}" = "1" ] && return 0
    local here lib unit_dir
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    lib="$HOME/.local/lib/nexus"
    unit_dir="$HOME/.config/systemd/user"
    mkdir -p "$lib" "$unit_dir" \
        && install -m 0755 "$here/log-rotate.sh" "$lib/log-rotate.sh" \
        && install -m 0644 "$here/systemd/nexus-log-rotate.service" "$here/systemd/nexus-log-rotate.timer" "$unit_dir/" \
        && systemctl --user daemon-reload \
        && systemctl --user enable --now nexus-log-rotate.timer \
        || warn "could not install the log-rotation timer (user systemd unavailable?) - native logs will not rotate"
}

cmd_start() {
    # Cloud's protected file is authoritative. Validate all required Cloud and
    # storage credentials, then adopt the Cloud registration URL/key into this
    # shell's inherited environment for dependent services. The key is never
    # passed as a KEY=value launcher argument.
    validate_cloud_environment || return 1
    adopt_cloud_registration_environment_for_dependents || return 1

    # The dashboard files issue reports to GitHub on a signed-in user's behalf,
    # which needs a token with Issues: write on the Nexus-Systems repo. Same
    # adoption pattern as Cloud's key, and for the same reason: passing an
    # unset variable explicitly would set it to empty in the child and override
    # the .env that bun would otherwise load, turning "configured" into
    # "silently unconfigured".
    #
    # Not a hard stop. Without it /api/issues answers 503 explaining itself and
    # everything else in the dashboard works, so a missing token is a degraded
    # feature rather than a reason to refuse to boot.
    if [ -z "${NEXUS_ISSUES_TOKEN:-}" ] && [ -f "$ROOT/apps/Nexus-Dashboard/.env" ]; then
        NEXUS_ISSUES_TOKEN="$(sed -n 's/^NEXUS_ISSUES_TOKEN=//p' "$ROOT/apps/Nexus-Dashboard/.env" \
            | head -1 | tr -d '\r' | sed -e 's/^"//' -e 's/"$//' -e "s/^'//" -e "s/'\$//")"
        [ -n "$NEXUS_ISSUES_TOKEN" ] && log "Adopted NEXUS_ISSUES_TOKEN from apps/Nexus-Dashboard/.env"
    fi
    export NEXUS_ISSUES_TOKEN="${NEXUS_ISSUES_TOKEN:-}"
    [ -z "$NEXUS_ISSUES_TOKEN" ] && log "NEXUS_ISSUES_TOKEN unset — in-app issue reporting will answer 503"

    # Calendar's private hop secret.
    #
    # Calendar is loopback-only and has no session of its own: Dashboard tells
    # it who is asking via x-nexus-subject. That header is a bare string, so
    # Calendar only believes it when the request also carries this secret —
    # which is why it must reach BOTH services and must never reach a browser
    # or any frontend build.
    #
    # Generated once and persisted beside the app, same as every other
    # credential here. 32 bytes of urandom, hex-encoded.
    if [ -z "${NEXUS_CALENDAR_DASHBOARD_SECRET:-}" ] && [ -f "$ROOT/apps/Nexus-Calendar/.env" ]; then
        NEXUS_CALENDAR_DASHBOARD_SECRET="$(sed -n 's/^NEXUS_CALENDAR_DASHBOARD_SECRET=//p' "$ROOT/apps/Nexus-Calendar/.env" \
            | head -1 | tr -d '\r' | sed -e 's/^"//' -e 's/"$//' -e "s/^'//" -e "s/'\$//")"
        [ -n "$NEXUS_CALENDAR_DASHBOARD_SECRET" ] && log "Adopted NEXUS_CALENDAR_DASHBOARD_SECRET from apps/Nexus-Calendar/.env"
    fi
    if [ -z "${NEXUS_CALENDAR_DASHBOARD_SECRET:-}" ]; then
        NEXUS_CALENDAR_DASHBOARD_SECRET="$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')"
        umask 077
        printf 'NEXUS_CALENDAR_DASHBOARD_SECRET=%s\n' "$NEXUS_CALENDAR_DASHBOARD_SECRET" \
            >> "$ROOT/apps/Nexus-Calendar/.env"
        log "Generated NEXUS_CALENDAR_DASHBOARD_SECRET into apps/Nexus-Calendar/.env"
    fi
    # Calendar trusts the subject only when this accompanies it, so a malformed
    # value (a truncated .env line, a stray quote) must stop the deploy rather
    # than start two services that silently cannot talk to each other.
    if ! [[ "$NEXUS_CALENDAR_DASHBOARD_SECRET" =~ ^[A-Fa-f0-9]{64}$ ]]; then
        echo "NEXUS_CALENDAR_DASHBOARD_SECRET must be a 32-byte hex secret" >&2
        return 1
    fi
    export NEXUS_CALENDAR_DASHBOARD_SECRET

    # The owner for events that predate ownership.
    #
    # Calendar refuses to start when it finds unowned rows and this is unset —
    # deliberately. Those events have no recorded owner and none can be
    # inferred, and the alternative to stopping is guessing, which means handing
    # one person's calendar to another. Set it to the subject that should own
    # them (an Auth user id, e.g. usr-xxxxxxxx-N) and the migration backfills
    # once, after which it is never read again.
    export NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT="${NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT:-}"
    if [ -z "$NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT" ]; then
        warn "NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT unset — calendar will refuse to start if any pre-ownership events remain"
    fi

    # Same pattern for Email's database URL: the value lives beside the app so
    # a credential is never written into this script or the repo.
    if [ -z "${NEXUS_EMAIL_DATABASE_URL:-}" ] && [ -f "$ROOT/apps/Nexus-Email/.env" ]; then
        NEXUS_EMAIL_DATABASE_URL="$(sed -n 's/^NEXUS_EMAIL_DATABASE_URL=//p' "$ROOT/apps/Nexus-Email/.env" \
            | head -1 | tr -d '\r"')"
        [ -n "$NEXUS_EMAIL_DATABASE_URL" ] && log "Adopted NEXUS_EMAIL_DATABASE_URL from apps/Nexus-Email/.env"
    fi

    # Auth's audit-log database URL, same pattern: the value lives beside the app.
    if [ -z "${NEXUS_AUTH_AUDIT_DATABASE_URL:-}" ] && [ -f "$ROOT/apps/Nexus-Auth/.env" ]; then
        NEXUS_AUTH_AUDIT_DATABASE_URL="$(sed -n 's/^NEXUS_AUTH_AUDIT_DATABASE_URL=//p' "$ROOT/apps/Nexus-Auth/.env" \
            | head -1 | tr -d '\r"')"
        [ -n "$NEXUS_AUTH_AUDIT_DATABASE_URL" ] && log "Adopted NEXUS_AUTH_AUDIT_DATABASE_URL from apps/Nexus-Auth/.env"
    fi

    # Email's bridge secrets (Cloudflare ingress token, Resend SMTP relay) come
    # from the same file. They are exported only around the one service that
    # needs each, never passed as KEY=value arguments, so they stay out of argv
    # (visible to every user via ps) and out of unrelated services' environment.
    for key in NEXUS_EMAIL_CLOUDFLARE_INGRESS_TOKEN NEXUS_EMAIL_EGRESS NEXUS_EMAIL_SMTP_HOST \
        NEXUS_EMAIL_SMTP_PORT NEXUS_EMAIL_SMTP_USER NEXUS_EMAIL_SMTP_PASS \
        NEXUS_EMAIL_DKIM_KEY_PATH NEXUS_EMAIL_DKIM_SELECTOR; do
        if [ -z "${!key:-}" ] && [ -f "$ROOT/apps/Nexus-Email/.env" ]; then
            printf -v "$key" '%s' "$(sed -n "s/^$key=//p" "$ROOT/apps/Nexus-Email/.env" | head -1 | tr -d '\r"')"
        fi
    done

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
    export NEXUS_AUTH_AUDIT_DATABASE_URL
    start_service "auth" "$ROOT/apps/Nexus-Auth" 4310 \
        PORT=4310 \
        NEXUS_AUTH_BASE_URL="http://127.0.0.1:4310" \
        NEXUS_AUTH_COOKIE_DOMAIN=".$DOMAIN" \
        NEXUS_CLOUD_URL=http://localhost:8787 \
        bun run src/index.ts
    export -n NEXUS_AUTH_AUDIT_DATABASE_URL

    # 3. Nexus Cloud
    #
    # NEXUS_CLOUD_DOMAIN must track $DOMAIN. It is the base Cloud mints public
    # subdomains under, the zone /api/v1/routes is keyed by, and the only suffix
    # /api/v1/routes/tls-ask will authorise a certificate for. Left unset it
    # defaults to "nexus.local", so Cloud would publish *.nexus.local routes
    # that the proxy — serving $DOMAIN — rejects as foreign hosts.
    #
    # CF_API_TOKEN / CF_ZONE_ID come from the protected Cloud environment loaded
    # by start-cloud.sh after the launcher chain. With a Zone:DNS:Edit token,
    # Cloud publishes DNS for custom domains itself — as proxied CNAMEs to the
    # tunnel, NOT A records: this
    # node has no routable public IP. NEXUS_TUNNEL_ID is the tunnel every hostname
    # is CNAMEd to; it is this node's "Nexus Systems" tunnel and is not secret (it
    # is visible in every cfargotunnel DNS record). tnhc.dev subdomains need no
    # per-name record — the *.tnhc.dev wildcard already covers them — so this only
    # matters for out-of-wildcard custom domains.
    start_cloud_service

    # 4. (removed) apps/Nexus-Team-Chat — the Bun scaffold chat. The real
    # chat is nexus-chat (apps/Nexus), started in 4b below. The scaffold can
    # come back from git history if it is ever revived.

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
            start_nexus_chat_service

            # Front door: SPA + /api + /gateway + /voice/ws on one origin.
            # Plain HTTP on 8095 — Cloudflare terminates TLS at the edge and
            # nothing here may hold 80/443.
            start_nexus_chat_web_service || warn "Nexus Chat Caddy did not start"
        fi
    else
        warn "deploy/production/nexus-chat.env absent — skipping nexus-chat"
    fi

    # 4d-1. Nexus-Draw front door (Caddy) — draw.tnhc.dev
    #
    # Joins the Draw SPA (static files) with the Draw backend API (port 3075)
    # and WebSocket collaboration endpoint into a single origin for the proxy.
    #
    # Requires the Draw frontend to be built: apps/Nexus-Draw/frontend/dist
    if command -v "$CADDY_BIN" >/dev/null 2>&1; then
        if [ -f "$ROOT/apps/Nexus-Draw/frontend/dist/index.html" ]; then
            start_service "nexus-draw-web" "$ROOT" 8091 \
                "$CADDY_BIN" run --config "$ROOT/deploy/production/nexus-draw.Caddyfile" --adapter caddyfile
        else
            warn "draw UI not built — run: (cd apps/Nexus-Draw/frontend && npm install && npm run build)"
        fi
    else
        warn "caddy not installed — draw.$DOMAIN has no front door"
    fi

    # 4d-2. Nexus-Calendar — calendar.tnhc.dev
    #
    # Backend API on 3068 (SQLite events), Caddy front door on 8092 joining
    # SPA + API into one origin. Same pattern as draw.
    if [ -f "$ROOT/apps/Nexus-Calendar/package.json" ]; then
        # Calendar's SQLite location is explicit in the production process
        # contract, rather than relying on the service working directory. A
        # populated legacy database still requires an operator-provided owner;
        # omit that variable entirely when it has not been configured so the
        # service retains its fail-closed migration behavior.
        local calendar_db="${NEXUS_CALENDAR_DB:-$ROOT/data/calendar.sqlite}"
        local calendar_jwt_audience="${NEXUS_CALENDAR_JWT_AUDIENCE:-calendar.$DOMAIN}"
        local calendar_web_root="${NEXUS_CALENDAR_WEB_ROOT:-$ROOT/apps/Nexus-Calendar/frontend/dist}"
        local -a calendar_env=(
            PORT=3068
            NEXUS_BIND_HOST=127.0.0.1
            NEXUS_CALENDAR_DB="$calendar_db"
            NEXUS_CALENDAR_JWT_AUDIENCE="$calendar_jwt_audience"
            NEXUS_CALENDAR_DASHBOARD_SECRET="$NEXUS_CALENDAR_DASHBOARD_SECRET"
            NEXUS_CALENDAR_BASE_URL=http://127.0.0.1:8092
            NEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310
            NEXUS_CLOUD_URL=http://localhost:8787
        )
        if [ -n "${NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT:-}" ]; then
            calendar_env+=(NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT="$NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT")
        fi
        start_service "calendar" "$ROOT/apps/Nexus-Calendar" 3068 \
            "${calendar_env[@]}" \
            bun run src/index.ts
        if command -v "$CADDY_BIN" >/dev/null 2>&1; then
            if [ -f "$ROOT/apps/Nexus-Calendar/frontend/dist/index.html" ]; then
                start_service "nexus-calendar-web" "$ROOT" 8092 \
                    NEXUS_CALENDAR_WEB_ROOT="$calendar_web_root" \
                    "$CADDY_BIN" run --config "$ROOT/deploy/production/nexus-calendar.Caddyfile" --adapter caddyfile
            else
                warn "calendar UI not built — run: (cd apps/Nexus-Calendar/frontend && pnpm install && pnpm run build)"
            fi
        fi
    fi

    # 4d. Nexus-Terminal — the loopback-only shell service used by Dashboard.
    #
    # The browser never reaches this port directly. Dashboard authorizes the
    # same founder/admin session, validates the browser Origin, and relays the
    # socket to this fixed loopback upstream. The shell remains disabled unless
    # an operator explicitly exports NEXUS_TERMINAL_ENABLED=true; deploying the
    # service must not implicitly grant host-shell access.
    start_service "terminal" "$ROOT/apps/Nexus-Terminal" 3110 \
        PORT=3110 \
        NEXUS_BIND_HOST=127.0.0.1 \
        NEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310 \
        NEXUS_CLOUD_URL=http://127.0.0.1:8787 \
        NEXUS_NEXUS_TERMINAL_BASE_URL=http://127.0.0.1:3110 \
        NEXUS_TERMINAL_ENABLED="${NEXUS_TERMINAL_ENABLED:-false}" \
        bun run src/index.ts

    # 4e. Nexus-Dashboard — app.$DOMAIN, the ecosystem front door.
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
            NEXUS_TERMINAL_URL=http://127.0.0.1:3110 \
            NEXUS_CALENDAR_URL=http://127.0.0.1:3068 \
            NEXUS_CALENDAR_DASHBOARD_SECRET="$NEXUS_CALENDAR_DASHBOARD_SECRET" \
            NEXUS_CALENDAR_WEB_URL=http://127.0.0.1:8092 \
            NEXUS_ISSUES_TOKEN="$NEXUS_ISSUES_TOKEN" \
            bun run src/index.ts
    fi

    # 4f. Nexus-Draw backend — the board/collab API behind draw.$DOMAIN.
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
        bun run src/index.ts

    # 4f-2. Nexus-API — the unified API server (apps/Nexus-API).
    #
    # The full Express API (sites, deploys, federation, storage) that used to
    # live inside Nexus-Hosting. Loopback only: it trusts session cookies the
    # way mailapi trusts X-Nexus-Subject, and nothing outside this host has a
    # reason to reach it directly. Registers with Cloud as nexus-api, which is
    # what flips its dashboard tile from offline.
    if [ -z "${DATABASE_URL:-}" ] && [ -f "$ROOT/.env" ]; then
        DATABASE_URL="$(sed -n 's/^POSTGRES_PASSWORD=//p' "$ROOT/.env" | head -1 | tr -d '\r')"
        export DATABASE_URL="postgres://nexus:${DATABASE_URL}@localhost:5432/nexus"
    fi
    if [ -x "$ROOT/apps/Nexus-API/node_modules/.bin/tsx" ]; then
        # OIDC credentials live in apps/Nexus-API/.env (gitignored, 0600) —
        # same pattern as nexus-chat.env. Sourced in a subshell so nothing
        # leaks into later services.
        API_ENV=""
        [ -f "$ROOT/apps/Nexus-API/.env" ] && API_ENV="$ROOT/apps/Nexus-API/.env"
        start_service "api" "$ROOT/apps/Nexus-API" 3150 \
            PORT=3150 \
            NEXUS_API_ENV_FILE="$API_ENV" \
            NEXUS_BIND_HOST=127.0.0.1 \
            DATABASE_URL="${DATABASE_URL:-postgres://127.0.0.1:1/nexus-unconfigured}" \
            OBJECT_STORAGE_ENDPOINT=http://localhost:9000 \
            OBJECT_STORAGE_BUCKET=nexus-hosting \
            DEFAULT_OBJECT_STORAGE_BUCKET_ID=nexus-hosting \
            MINIO_ROOT_USER="${MINIO_ROOT_USER:-nexus-storage}" \
            MINIO_ROOT_PASSWORD="${MINIO_ROOT_PASSWORD:-}" \
            REDIS_URL=redis://127.0.0.1:6379 \
            NEXUS_CLOUD_URL=http://localhost:8787 \
            PUBLIC_URL=http://127.0.0.1:3150 \
            bash -c 'if [ -n "$NEXUS_API_ENV_FILE" ]; then set -a; . "$NEXUS_API_ENV_FILE"; set +a; fi; exec node --import tsx src/index.ts' 
    else
        warn "nexus-api deps missing — run: (cd apps/Nexus-API && pnpm install)"
    fi

    # 4g. Nexus-Email API — the mail store behind app.$DOMAIN/mail.
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
        # The one public path on this service is the Cloudflare Email Worker
        # ingress, which the proxy pins (email-ingress.$DOMAIN, POST only) and
        # mailapi refuses unless the bearer token below is set and matches.
        export NEXUS_EMAIL_CLOUDFLARE_INGRESS_TOKEN
        start_service "mailapi" "$ROOT/apps/Nexus-Email" 3140 \
            NEXUS_EMAIL_BIND=127.0.0.1:3140 \
            NEXUS_EMAIL_DOMAIN="$DOMAIN" \
            NEXUS_EMAIL_DATABASE_URL="$NEXUS_EMAIL_DATABASE_URL" \
            "$MAILAPI_BIN"
        export -n NEXUS_EMAIL_CLOUDFLARE_INGRESS_TOKEN
    else
        log "mailapi binary not built - skipping (cargo build -p nexus-mailapi)"
    fi

    # 4h. Nexus-Email SMTP daemon.
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
    # IMAP runs in the same process on 2143, so ordinary mail clients can use
    # a Nexus mailbox. Credentials are verified against Auth, never stored here.
    # It is plaintext on loopback: there is no TLS in this daemon, so exposing
    # it beyond localhost needs a TLS terminator in front.
    #
    # The same process runs the delivery worker and the federation listener
    # (2580, loopback): pinned peers hand mail over it, and with
    # NEXUS_EMAIL_EGRESS=peer:<domain> outside mail leaves through that peer
    # instead of this host's filtered port 25. Peers are pinned with
    # nexus-mailctl; see apps/Nexus-Email/README.md.
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
        export NEXUS_EMAIL_SMTP_HOST NEXUS_EMAIL_SMTP_PORT NEXUS_EMAIL_SMTP_USER NEXUS_EMAIL_SMTP_PASS
        start_service "mailsmtpd" "$ROOT/apps/Nexus-Email" "${NEXUS_EMAIL_MX_PORT:-2525}" \
            NEXUS_EMAIL_MX_BIND="${NEXUS_EMAIL_MX_BIND:-127.0.0.1:2525}" \
            NEXUS_EMAIL_SUBMISSION_BIND="${NEXUS_EMAIL_SUBMISSION_BIND:-127.0.0.1:2587}" \
            NEXUS_EMAIL_IMAP_BIND="${NEXUS_EMAIL_IMAP_BIND:-127.0.0.1:2143}" \
            NEXUS_AUTH_INTERNAL_URL="${NEXUS_AUTH_INTERNAL_URL:-http://127.0.0.1:4310}" \
            NEXUS_EMAIL_DOMAIN="$DOMAIN" \
            NEXUS_EMAIL_HOSTNAME="mail.$DOMAIN" \
            NEXUS_EMAIL_POLICY="${NEXUS_EMAIL_POLICY:-observe}" \
            NEXUS_EMAIL_DATABASE_URL="$NEXUS_EMAIL_DATABASE_URL" \
            NEXUS_EMAIL_FEDERATION_BIND="${NEXUS_EMAIL_FEDERATION_BIND:-127.0.0.1:2580}" \
            NEXUS_EMAIL_FEDERATION_HOST="${NEXUS_EMAIL_FEDERATION_HOST:-mail.$DOMAIN}" \
            NEXUS_EMAIL_NODE_KEY_PATH="${NEXUS_EMAIL_NODE_KEY_PATH:-}" \
            NEXUS_EMAIL_EGRESS="${NEXUS_EMAIL_EGRESS:-direct}" \
            NEXUS_EMAIL_DKIM_KEY_PATH="${NEXUS_EMAIL_DKIM_KEY_PATH:-}" \
            NEXUS_EMAIL_DKIM_SELECTOR="${NEXUS_EMAIL_DKIM_SELECTOR:-}" \
            "$SMTPD_BIN"
        export -n NEXUS_EMAIL_SMTP_HOST NEXUS_EMAIL_SMTP_PORT NEXUS_EMAIL_SMTP_USER NEXUS_EMAIL_SMTP_PASS
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
        DASHBOARD_UPSTREAM=http://127.0.0.1:3132 \
        HOSTING_SITE_UPSTREAM="${HOSTING_SITE_UPSTREAM:-http://127.0.0.1:8090}" \
        bun run proxy.ts

    install_log_rotation

    # 6. Verify
    sleep 3
    cmd_status
}

stop_service() {
    local svc=$1
    local pid port state attempt

    service_identity "$svc" || return 1
    port="$(service_port "$svc")"
    if ! pid="$(validated_pid "$svc" "$port" "$SERVICE_DIR" "$SERVICE_EXEC_PATTERN" 2>/dev/null)"; then
        # Recover an exact late-start listener even if its launcher missed the
        # PID checkpoint. Foreign/mismatched listeners still fail reconciliation.
        pid="$(reconcile_pid "$svc" "$port" "$SERVICE_DIR" "$SERVICE_EXEC_PATTERN" 2>/dev/null)" || pid=
    fi
    if [ -z "$pid" ]; then
        state="$(listener_state "$port" 2>/dev/null)"
        if [ "$state" = absent ] && [ ! -f "$PID_DIR/$svc.pid" ]; then
            log "  $svc is already stopped"
            return 0
        fi
        case "$state" in
            occupied) warn "  Left $svc untouched: :$port is occupied by an unverified or mismatched process" ;;
            unverifiable) warn "  Left $svc untouched: cannot inspect :$port safely" ;;
            absent) warn "  Left $svc untouched: its PID file does not match a listening service" ;;
            *) warn "  Left $svc untouched: listener state is not safely known" ;;
        esac
        return 1
    fi

    if ! "$KILL_BIN" "$pid" 2>/dev/null; then
        warn "  Could not stop managed $svc (PID: $pid)"
        return 1
    fi
    for ((attempt = 1; attempt <= ${NEXUS_PRODUCTION_STOP_WAIT_ATTEMPTS:-20}; attempt++)); do
        if ! kill -0 "$pid" 2>/dev/null; then
            rm -f -- "$PID_DIR/$svc.pid"
            log "  Stopped $svc"
            return 0
        fi
        sleep "${NEXUS_PRODUCTION_STOP_WAIT_INTERVAL:-0.25}"
    done
    warn "  Managed $svc did not exit after the stop signal (PID: $pid); PID file preserved"
    return 1
}

cmd_restart_service() {
    local svc=${1:-}

    preflight_service_start "$svc" || return 1
    stop_service "$svc" || return 1
    start_named_service "$svc"
}

completed_rotation_dir() {
    local runtime_dir active_state candidate

    runtime_dir="${NEXUS_ROTATION_RUNTIME_DIR:-/tmp/nexus-production}"
    active_state="$runtime_dir/rotation.current"
    [ -f "$active_state" ] || {
        warn "Active credential-rotation state is missing; refusing sensitive-log cleanup"
        return 1
    }
    IFS= read -r candidate < "$active_state" || true
    case "$candidate" in
        "$runtime_dir"/rotation.??????) ;;
        *) warn "Active credential-rotation state is invalid; refusing sensitive-log cleanup"; return 1 ;;
    esac
    [ -d "$candidate" ] || {
        warn "Active credential-rotation directory is missing; refusing sensitive-log cleanup"
        return 1
    }
    [ -f "$candidate/tunnel.checkpoint" ] || {
        warn "Tunnel checkpoint is incomplete; preserving sensitive historical logs"
        return 1
    }
    [ -f "$candidate/storage.checkpoint" ] || {
        warn "Storage checkpoint is incomplete; preserving sensitive historical logs"
        return 1
    }
    printf '%s\n' "$candidate"
}

reset_sensitive_log_after_checkpoints() {
    local svc=$1
    local staged

    case "$svc" in
        cloud|nexus-chat-web) ;;
        *) warn "Sensitive-log cleanup is limited to cloud and nexus-chat-web"; return 1 ;;
    esac
    completed_rotation_dir >/dev/null || return 1
    staged="$(mktemp "$LOG_DIR/.${svc}.log.cleanup.XXXXXX")"
    chmod 600 "$staged"
    mv -f -- "$staged" "$LOG_DIR/$svc.log"
}

cmd_cleanup_sensitive_log() {
    local svc=${1:-}

    # All preconditions are checked while the existing writer and log remain
    # untouched. Only after both credential checkpoints and the replacement
    # service configuration are valid do we stop, replace the exact log, and
    # restart that one service.
    completed_rotation_dir >/dev/null || return 1
    preflight_service_start "$svc" || return 1
    stop_service "$svc" || return 1
    if ! reset_sensitive_log_after_checkpoints "$svc"; then
        warn "Rotation checkpoints changed during log cleanup; preserving the log and restarting $svc"
        start_named_service "$svc" || true
        return 1
    fi
    start_named_service "$svc"
}

cmd_stop() {
    local svc

    log "Stopping all Nexus services..."
    for svc in auth cloud api calendar nexus-calendar-web nexus-chat nexus-chat-web nexus-draw-web terminal dashboard draw mailapi mailsmtpd proxy; do
        stop_service "$svc" || true
    done
    cd "$ROOT"
    docker-compose down
    log "All stopped"
}

cmd_service_status() {
    echo "Service Status:"
    for svc in auth cloud api calendar nexus-calendar-web nexus-chat nexus-chat-web nexus-draw-web terminal dashboard draw mailapi mailsmtpd proxy; do
        service_identity "$svc" || continue
        local pid port
        port="$(service_port "$svc")"
        if pid="$(validated_pid "$svc" "$port" "$SERVICE_DIR" "$SERVICE_EXEC_PATTERN" 2>/dev/null)"; then
            echo -e "  ${G}● $svc${R} (managed, PID: $pid)"
        else
            local state
            state="$(listener_state "$port" 2>/dev/null)"
            case "$state" in
                occupied) echo -e "  ${Y}! $svc${R} (conflict on :$port)" ;;
                unverifiable) echo -e "  ${Y}! $svc${R} (unverifiable listener state on :$port)" ;;
                absent) echo -e "  ${R}? $svc${R} (not running)" ;;
                *) echo -e "  ${Y}! $svc${R} (unverifiable listener state on :$port)" ;;
            esac
        fi
    done
}

cmd_status() {
    cmd_service_status

    # Check HTTP endpoints
    echo ""
    echo "HTTP Health Checks:"
    # nexus-chat answers /api/v1/health, not /health, and is probed through its
    # Caddy front door on 8095 — that is the origin chat.$DOMAIN actually
    # reaches, so a healthy API behind a dead front door still reads as down.
    # nexus-draw-web is similar: the Draw API is on 3075, but the front door
    # on 8091 joins the SPA + API + WebSocket, so probe the front door.
    for endpoint in "http://localhost:4310/health" "http://localhost:8787/health" "http://127.0.0.1:3150/api/health/live" "http://localhost:3068/health" "http://localhost:8092/health" "http://localhost:8095/api/v1/health" "http://localhost:8091/health" "http://127.0.0.1:3110/health" "http://localhost:3132/health" "http://localhost:3075/health" "http://localhost:8080/health"; do
        if http_endpoint_healthy "$endpoint"; then
            echo -e "  ${G}●${R} $endpoint"
        else
            echo -e "  ${R}✗${R} $endpoint"
        fi
    done
}

http_endpoint_healthy() {
    local endpoint=$1
    local body

    if ! body="$("$CURL_BIN" --silent --show-error --fail --max-time 2 "$endpoint")"; then
        return 1
    fi
    case "$body" in
        ok|healthy) return 0 ;;
    esac
    printf '%s' "$body" | jq -e '
        type == "object" and (
            .ok == true
            or .healthy == true
            or .status == "ok"
            or .status == "healthy"
        )
    ' >/dev/null 2>&1
}

usage() {
    cat <<USAGE
Usage: $(basename "$0") [command]

  (no command)      start in the foreground, Ctrl+C to stop
  bg, --bg          start in the background and return
  stop, --stop      stop all services and the infrastructure containers
  status, --status  report what is running
  restart SERVICE   safely restart cloud, nexus-chat, or nexus-chat-web
  cleanup-log SERVICE
                    after both rotation checkpoints, stop one affected writer,
                    replace its exact historical log, and safely restart it

Environment: DOMAIN (default tnhc.dev), NEXUS_AUTH_PUBLIC_URL
USAGE
}

# Unrecognised arguments must not fall through to cmd_start. They used to: only
# the ---prefixed spellings were matched, so `deploy.sh status` — the spelling
# anyone tries first — deployed production and then sat in the foreground loop
# below, and any typo did the same. An unknown argument now fails without
# touching anything, and the bare words are accepted alongside the flags.
main() {
    case "${1:-}" in
        "")             cmd_start; echo "Press Ctrl+C to stop"; while true; do sleep 1; done ;;
        bg|--bg)        cmd_start; echo "Services started. Logs: $LOG_DIR/*.log" ;;
        stop|--stop)    cmd_stop ;;
        status|--status) cmd_status ;;
        restart)        [ "$#" -eq 2 ] || { usage >&2; return 2; }; cmd_restart_service "$2" ;;
        cleanup-log)    [ "$#" -eq 2 ] || { usage >&2; return 2; }; cmd_cleanup_sensitive_log "$2" ;;
        -h|--help|help) usage ;;
        *)              echo "Unknown command: $1" >&2; usage >&2; return 2 ;;
    esac
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
