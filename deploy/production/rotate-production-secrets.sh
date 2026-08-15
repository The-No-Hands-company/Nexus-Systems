#!/usr/bin/env bash
# Checkpointed production credential rotation. Secret values stay in mode-0600
# files or redirected stdin; command arguments and status output remain safe.

set -euo pipefail
set -E
umask 077

# Capture an operator-supplied token before running any child process, then
# remove it from the exported environment. Cloudflare authentication is passed
# to curl only through a protected config file.
CF_API_TOKEN_INPUT=${CF_API_TOKEN:-}
unset CF_API_TOKEN

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNTIME_DIR="${NEXUS_ROTATION_RUNTIME_DIR:-/tmp/nexus-production}"
SECRETS_DIR="$RUNTIME_DIR/secrets"
ROOT_ENV="${NEXUS_ROTATION_ROOT_ENV:-$ROOT/.env}"
CLOUD_ENV="${NEXUS_ROTATION_CLOUD_ENV:-$ROOT/apps/Nexus-Cloud/.env}"
TUNNEL_TOKEN_FILE="${NEXUS_ROTATION_TUNNEL_TOKEN_FILE:-$SECRETS_DIR/cloudflared.token}"
ACTIVE_STATE="$RUNTIME_DIR/rotation.current"
ROTATION_LOCK_FILE="${NEXUS_ROTATION_LOCK_FILE:-$RUNTIME_DIR/rotation.lock}"
CLOUDFLARED_COMPOSE="${NEXUS_ROTATION_CLOUDFLARED_COMPOSE:-$ROOT/deploy/production/cloudflared.compose.yml}"
INFRA_COMPOSE="${NEXUS_ROTATION_INFRA_COMPOSE:-$ROOT/docker-compose.yml}"
CF_API_BASE="${NEXUS_ROTATION_CF_API_BASE:-https://api.cloudflare.com/client/v4}"
MINIO_CONTAINER="${NEXUS_ROTATION_MINIO_CONTAINER:-nexus-systems-minio-1}"
MINIO_HEALTH_ATTEMPTS="${NEXUS_ROTATION_MINIO_HEALTH_ATTEMPTS:-30}"
MINIO_HEALTH_INTERVAL="${NEXUS_ROTATION_MINIO_HEALTH_INTERVAL:-2}"
TUNNEL_VERIFY_URL="${NEXUS_ROTATION_TUNNEL_VERIFY_URL:-}"
TUNNEL_CONNECT_ATTEMPTS="${NEXUS_ROTATION_TUNNEL_CONNECT_ATTEMPTS:-30}"
TUNNEL_CONNECT_INTERVAL="${NEXUS_ROTATION_TUNNEL_CONNECT_INTERVAL:-2}"

ROTATION_DIR=
STORAGE_PENDING=0
TUNNEL_RECOVERY_PENDING=0
TUNNEL_STAGED_TOKEN=
TUNNEL_ACCOUNT_ID=
TUNNEL_ID=
TUNNEL_OLD_CONNECTION_IDS=
EXIT_GUARD=0
ROTATION_LOCK_FD=

log() {
    printf '[rotation] %s\n' "$*"
}

warn() {
    printf '[rotation] ERROR: %s\n' "$*" >&2
}

die() {
    warn "$*"
    exit 1
}

usage() {
    cat <<'EOF'
Usage: rotate-production-secrets.sh PHASE

Phases:
  prepare         Back up both environment files in protected rotation state.
  rotate-tunnel   Adopt the token produced by an operator's completed dashboard
                  rotation, verify its connector, then disconnect old clients.
  rotate-storage  Replace paired MinIO/Cloud credentials, recreate MinIO, and
                  roll both files back if MinIO misses the health checkpoint.
  cleanup         Remove the active protected rollback directory after the
                  operator has completed all invalidation and health checks.
EOF
}

require_absolute_runtime_dir() {
    case "$RUNTIME_DIR" in
        /*) ;;
        *) die "NEXUS_ROTATION_RUNTIME_DIR must be an absolute path" ;;
    esac
}

ensure_runtime_dirs() {
    require_absolute_runtime_dir
    install -d -m 700 "$RUNTIME_DIR" "$SECRETS_DIR"
}

acquire_rotation_lock() {
    ensure_runtime_dirs
    exec {ROTATION_LOCK_FD}>"$ROTATION_LOCK_FILE"
    chmod 600 "$ROTATION_LOCK_FILE"
    flock -n "$ROTATION_LOCK_FD" || die "another rotation operator is active"
}

require_file() {
    local file=$1
    local label=$2
    [ -f "$file" ] || die "$label is missing: $file"
}

load_rotation_dir() {
    local candidate

    ensure_runtime_dirs
    require_file "$ACTIVE_STATE" "active rotation state"
    IFS= read -r candidate < "$ACTIVE_STATE" || true
    case "$candidate" in
        "$RUNTIME_DIR"/rotation.*) ;;
        *) die "active rotation state points outside the runtime directory" ;;
    esac
    [ -d "$candidate" ] || die "active rotation directory is missing"
    ROTATION_DIR=$candidate
}

read_env_value_into() {
    local target=$1
    local file=$2
    local key=$3
    local line extracted

    while IFS= read -r line || [ -n "$line" ]; do
        line=${line%$'\r'}
        case "$line" in
            "$key="*)
                extracted=${line#*=}
                if [[ "$extracted" == \"*\" ]] && [ "${#extracted}" -ge 2 ]; then
                    extracted=${extracted:1:${#extracted}-2}
                elif [[ "$extracted" == \'*\' ]] && [ "${#extracted}" -ge 2 ]; then
                    extracted=${extracted:1:${#extracted}-2}
                fi
                printf -v "$target" '%s' "$extracted"
                return 0
                ;;
        esac
    done < "$file"
    return 1
}

required_env_value_into() {
    local target=$1
    local exported_value=$2
    local file=$3
    local key=$4
    local value

    value=$exported_value
    if [ -z "$value" ]; then
        read_env_value_into value "$file" "$key" || true
    fi
    [ -n "$value" ] || die "$key is required"
    printf -v "$target" '%s' "$value"
}

write_cloudflare_auth_config() {
    local token=$1
    local output=$2
    local staged="$output.next"

    : > "$staged"
    chmod 600 "$staged"
    printf 'header = "Authorization: Bearer %s"\n' "$token" > "$staged"
    mv -f -- "$staged" "$output"
}

cf_request() {
    local method=$1
    local url=$2
    local output=$3
    local data_file=${4:-}
    local auth_config="$ROTATION_DIR/cloudflare.curl-config"
    local -a args

    install -m 600 /dev/null "$output"
    args=(
        --silent
        --show-error
        --fail-with-body
        --config "$auth_config"
        --request "$method"
        --output "$output"
    )
    if [ -n "$data_file" ]; then
        args+=(--header 'Content-Type: application/json' --data-binary "@$data_file")
    fi
    curl "${args[@]}" "$url"
    jq -e '.success == true' "$output" >/dev/null
}

public_request() {
    local url=$1
    local output=$2

    install -m 600 /dev/null "$output"
    curl --silent --show-error --fail --output "$output" "$url"
}

install_tunnel_token() {
    local staged_token=$1

    require_file "$staged_token" "replacement tunnel token"
    install -d -m 700 "$(dirname "$TUNNEL_TOKEN_FILE")"
    install -m 600 "$staged_token" "$TUNNEL_TOKEN_FILE.next"
    mv -f -- "$TUNNEL_TOKEN_FILE.next" "$TUNNEL_TOKEN_FILE"
}

recreate_tunnel_connector() {
    # The pre-containment container may have been created with `docker run` and
    # therefore cannot be adopted by Compose. Remove only its exact name.
    docker rm -f cloudflared >/dev/null 2>&1 || true
    CLOUDFLARED_TOKEN_FILE="$TUNNEL_TOKEN_FILE" \
        docker compose -f "$CLOUDFLARED_COMPOSE" up -d --force-recreate cloudflared
    [ "$(docker inspect --format '{{.State.Running}}' cloudflared)" = true ] || {
        warn "replacement cloudflared container is not running"
        return 1
    }
}

recover_tunnel() {
    local failed=0

    [ -n "$TUNNEL_STAGED_TOKEN" ] || {
        warn "replacement tunnel token is unavailable for recovery"
        return 1
    }
    warn "tunnel adoption failed after retrieving the dashboard token; preserving the new token and restoring its connector"
    install_tunnel_token "$TUNNEL_STAGED_TOKEN" || failed=1
    if [ "$failed" -eq 0 ]; then
        recreate_tunnel_connector || failed=1
    fi
    if [ "$failed" -eq 0 ]; then
        [ -n "$TUNNEL_ACCOUNT_ID" ] && [ -n "$TUNNEL_ID" ] && [ -n "$TUNNEL_OLD_CONNECTION_IDS" ] || failed=1
    fi
    if [ "$failed" -eq 0 ]; then
        wait_for_replacement_connection "$TUNNEL_ACCOUNT_ID" "$TUNNEL_ID" "$TUNNEL_OLD_CONNECTION_IDS" || failed=1
    fi
    if [ "$failed" -eq 0 ]; then
        public_request "$TUNNEL_VERIFY_URL" "$ROTATION_DIR/tunnel-public-recovery.response" || failed=1
    fi
    if [ "$failed" -eq 0 ]; then
        TUNNEL_RECOVERY_PENDING=0
        warn "replacement tunnel token and connector were restored; old connections were left untouched"
        return 0
    fi
    warn "replacement tunnel recovery requires immediate operator attention; do not reinstall the revoked rollback token"
    return 1
}

replace_env_value_from_file() {
    local env_file=$1
    local key=$2
    local value_file=$3
    local env_dir env_base staged installed line replacement found

    require_file "$env_file" "environment file"
    require_file "$value_file" "replacement value file"
    replacement=
    IFS= read -r replacement < "$value_file" || true
    [ -n "$replacement" ] || die "generated replacement for $key is empty"

    env_dir="$(dirname "$env_file")"
    env_base="$(basename "$env_file")"
    staged="$(mktemp "$env_dir/.${env_base}.rotation.XXXXXX")"
    installed="$env_dir/.${env_base}.rotation.install"
    found=0
    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            "$key="*)
                printf '%s=%s\n' "$key" "$replacement"
                found=1
                ;;
            *) printf '%s\n' "$line" ;;
        esac
    done < "$env_file" > "$staged"
    if [ "$found" -eq 0 ]; then
        printf '%s=%s\n' "$key" "$replacement" >> "$staged"
    fi
    install -m 600 "$staged" "$installed"
    mv -f -- "$installed" "$env_file"
    rm -f -- "$staged"
}

recreate_minio() {
    docker compose \
        --project-directory "$ROOT" \
        --env-file "$ROOT_ENV" \
        -f "$INFRA_COMPOSE" \
        up -d --force-recreate minio
}

wait_for_minio_health() {
    local attempt status

    for ((attempt = 1; attempt <= MINIO_HEALTH_ATTEMPTS; attempt++)); do
        status="$(docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}missing{{end}}' "$MINIO_CONTAINER" 2>/dev/null || true)"
        if [ "$status" = healthy ]; then
            return 0
        fi
        if [ "$attempt" -lt "$MINIO_HEALTH_ATTEMPTS" ]; then
            sleep "$MINIO_HEALTH_INTERVAL"
        fi
    done
    warn "MinIO did not reach the health checkpoint"
    return 1
}

restore_env_file() {
    local backup=$1
    local destination=$2
    local installed="$(dirname "$destination")/.$(basename "$destination").rollback.install"

    install -m 600 "$backup" "$installed"
    mv -f -- "$installed" "$destination"
}

rollback_storage() {
    local failed=0

    restore_env_file "$ROTATION_DIR/root.env.rollback" "$ROOT_ENV" || failed=1
    restore_env_file "$ROTATION_DIR/cloud.env.rollback" "$CLOUD_ENV" || failed=1
    if [ "$failed" -ne 0 ]; then
        warn "paired environment rollback could not be completed"
        return 1
    fi
    recreate_minio || {
        warn "environment files were restored but MinIO recreation failed"
        return 1
    }
    wait_for_minio_health || {
        warn "environment files were restored but MinIO did not become healthy"
        return 1
    }
}

on_exit() {
    local status=$1
    local recovery_status=0 pending_at_exit=0

    if [ "$EXIT_GUARD" -eq 1 ]; then
        exit "$status"
    fi
    EXIT_GUARD=1
    trap - EXIT ERR INT TERM HUP
    set +e
    if [ "$STORAGE_PENDING" -eq 1 ]; then
        pending_at_exit=1
        warn "storage rotation stopped before health; restoring both environment files"
        rollback_storage
        recovery_status=$?
        if [ "$recovery_status" -eq 0 ]; then
            STORAGE_PENDING=0
            warn "paired storage rollback completed after restored MinIO became healthy"
        else
            warn "paired storage rollback requires immediate operator attention"
        fi
    fi
    if [ "$TUNNEL_RECOVERY_PENDING" -eq 1 ]; then
        pending_at_exit=1
        recover_tunnel
        recovery_status=$?
    fi
    if [ "$status" -eq 0 ] && { [ "$pending_at_exit" -eq 1 ] || [ "$recovery_status" -ne 0 ]; }; then
        status=1
    fi
    exit "$status"
}

on_signal() {
    local signal_name=$1
    local status=$2

    warn "received $signal_name during credential rotation"
    exit "$status"
}

trap 'on_exit $?' EXIT
trap 'on_signal INT 130' INT
trap 'on_signal TERM 143' TERM
trap 'on_signal HUP 129' HUP

phase_prepare() {
    local rotation_dir state_source token_hash current_token normalized_token

    ensure_runtime_dirs
    require_file "$ROOT_ENV" "root environment file"
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"
    require_file "$TUNNEL_TOKEN_FILE" "current tunnel token file"
    if [ -e "$ACTIVE_STATE" ]; then
        die "an active rotation already exists; verify it and run cleanup first"
    fi

    rotation_dir="$(mktemp -d "$RUNTIME_DIR/rotation.XXXXXX")"
    chmod 700 "$rotation_dir"
    ROTATION_DIR=$rotation_dir
    install -m 600 "$ROOT_ENV" "$ROTATION_DIR/root.env.rollback"
    install -m 600 "$CLOUD_ENV" "$ROTATION_DIR/cloud.env.rollback"
    install -m 600 "$TUNNEL_TOKEN_FILE" "$ROTATION_DIR/cloudflared.token.rollback"
    current_token=
    IFS= read -r current_token < "$ROTATION_DIR/cloudflared.token.rollback" || true
    [ -n "$current_token" ] || die "current tunnel token file is empty"
    normalized_token="$ROTATION_DIR/cloudflared.token.rollback.normalized"
    printf '%s' "$current_token" > "$normalized_token"
    chmod 600 "$normalized_token"
    token_hash="$(sha256sum "$normalized_token" | awk '{print $1}')"
    rm -f -- "$normalized_token"
    current_token=
    printf '%s\n' "$token_hash" > "$ROTATION_DIR/cloudflared.token.rollback.sha256"
    chmod 600 "$ROTATION_DIR/cloudflared.token.rollback.sha256"
    state_source="$ROTATION_DIR/active-path"
    printf '%s\n' "$ROTATION_DIR" > "$state_source"
    install -m 600 "$state_source" "$ACTIVE_STATE"
    log "Prepared protected rollback state in $ROTATION_DIR"
}

snapshot_old_connection_ids() {
    local response=$1
    local output=$2

    install -m 600 /dev/null "$output"
    jq -er \
        '.result | arrays | .[]? | .id | select(type == "string" and test("^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"))' \
        "$response" > "$output" || {
        [ "$(jq -r '.result | arrays | length' "$response")" = 0 ] || return 1
    }
    chmod 600 "$output"
}

wait_for_replacement_connection() {
    local account_id=$1
    local tunnel_id=$2
    local old_ids=$3
    local attempt response active_ids client_id

    response="$ROTATION_DIR/tunnel-connections-current.json"
    active_ids="$ROTATION_DIR/tunnel-connections-active.ids"
    for ((attempt = 1; attempt <= TUNNEL_CONNECT_ATTEMPTS; attempt++)); do
        cf_request GET "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/connections" "$response"
        install -m 600 /dev/null "$active_ids"
        jq -r \
            '.result[]? | select(any(.conns[]?; .is_pending_reconnect == false)) | .id | select(type == "string" and test("^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"))' \
            "$response" > "$active_ids"
        while IFS= read -r client_id || [ -n "$client_id" ]; do
            [ -n "$client_id" ] || continue
            if ! grep -Fxq -- "$client_id" "$old_ids"; then
                return 0
            fi
        done < "$active_ids"
        if [ "$attempt" -lt "$TUNNEL_CONNECT_ATTEMPTS" ]; then
            sleep "$TUNNEL_CONNECT_INTERVAL"
        fi
    done
    warn "Cloudflare did not report an active replacement tunnel connection"
    return 1
}

phase_rotate_tunnel() {
    local api_token zone_id tunnel_id auth_config zone_response account_id
    local connections_response old_connection_ids token_response staged_token
    local old_hash new_hash public_response client_id delete_index

    load_rotation_dir
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"
    require_file "$ROTATION_DIR/cloudflared.token.rollback" "prepared tunnel token rollback"
    require_file "$ROTATION_DIR/cloudflared.token.rollback.sha256" "prepared tunnel token hash"
    required_env_value_into api_token "$CF_API_TOKEN_INPUT" "$CLOUD_ENV" CF_API_TOKEN
    required_env_value_into zone_id "${CF_ZONE_ID:-}" "$CLOUD_ENV" CF_ZONE_ID
    required_env_value_into tunnel_id "${NEXUS_TUNNEL_ID:-}" "$CLOUD_ENV" NEXUS_TUNNEL_ID
    [ -n "$TUNNEL_VERIFY_URL" ] || die "NEXUS_ROTATION_TUNNEL_VERIFY_URL is required"

    auth_config="$ROTATION_DIR/cloudflare.curl-config"
    write_cloudflare_auth_config "$api_token" "$auth_config"
    api_token=
    CF_API_TOKEN_INPUT=

    zone_response="$ROTATION_DIR/cloudflare-zone-response.json"
    cf_request GET "$CF_API_BASE/zones/$zone_id" "$zone_response"
    account_id="$(jq -er '.result.account.id | select(type == "string" and test("^[0-9A-Fa-f]{32}$"))' "$zone_response")"

    connections_response="$ROTATION_DIR/tunnel-connections-before.json"
    old_connection_ids="$ROTATION_DIR/tunnel-connections-old.ids"
    cf_request GET "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/connections" "$connections_response"
    snapshot_old_connection_ids "$connections_response" "$old_connection_ids"

    token_response="$ROTATION_DIR/tunnel-token-response.json"
    staged_token="$ROTATION_DIR/cloudflared.token.new"
    cf_request GET "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/token" "$token_response"
    jq -jer '.result | select(type == "string" and length > 0)' "$token_response" > "$staged_token"
    chmod 600 "$staged_token"

    IFS= read -r old_hash < "$ROTATION_DIR/cloudflared.token.rollback.sha256"
    new_hash="$(sha256sum "$staged_token" | awk '{print $1}')"
    [ -n "$old_hash" ] && [ "$new_hash" != "$old_hash" ] || \
        die "dashboard tunnel rotation is not visible yet; fetched token is unchanged"

    TUNNEL_STAGED_TOKEN=$staged_token
    TUNNEL_ACCOUNT_ID=$account_id
    TUNNEL_ID=$tunnel_id
    TUNNEL_OLD_CONNECTION_IDS=$old_connection_ids
    TUNNEL_RECOVERY_PENDING=1
    install_tunnel_token "$staged_token"
    recreate_tunnel_connector
    wait_for_replacement_connection "$account_id" "$tunnel_id" "$old_connection_ids"
    public_response="$ROTATION_DIR/tunnel-public-verification.response"
    public_request "$TUNNEL_VERIFY_URL" "$public_response"

    # The new token and connector are now verified. Recovery is no longer
    # needed, and only the client IDs observed before replacement are removed.
    TUNNEL_RECOVERY_PENDING=0
    delete_index=0
    while IFS= read -r client_id || [ -n "$client_id" ]; do
        [ -n "$client_id" ] || continue
        delete_index=$((delete_index + 1))
        cf_request DELETE \
            "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/connections?client_id=$client_id" \
            "$ROTATION_DIR/tunnel-connection-delete-$delete_index.json"
    done < "$old_connection_ids"
    install -m 600 /dev/null "$ROTATION_DIR/tunnel.checkpoint"
    log "Tunnel adoption reached the verified connector and old-connection cleanup checkpoint"
}

phase_rotate_storage() {
    local new_user new_password

    load_rotation_dir
    require_file "$ROTATION_DIR/root.env.rollback" "root environment rollback"
    require_file "$ROTATION_DIR/cloud.env.rollback" "Cloud environment rollback"
    require_file "$ROOT_ENV" "root environment file"
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"

    new_user="$ROTATION_DIR/minio-root-user.new"
    new_password="$ROTATION_DIR/minio-root-password.new"
    openssl rand -hex 16 > "$new_user"
    openssl rand -base64 32 > "$new_password"
    chmod 600 "$new_user" "$new_password"

    STORAGE_PENDING=1
    replace_env_value_from_file "$ROOT_ENV" MINIO_ROOT_USER "$new_user"
    replace_env_value_from_file "$ROOT_ENV" MINIO_ROOT_PASSWORD "$new_password"
    replace_env_value_from_file "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY "$new_user"
    replace_env_value_from_file "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY "$new_password"
    recreate_minio
    wait_for_minio_health
    STORAGE_PENDING=0
    install -m 600 /dev/null "$ROTATION_DIR/storage.checkpoint"
    log "Storage rotation reached the paired MinIO health checkpoint"
}

phase_cleanup() {
    local doomed

    load_rotation_dir
    doomed=$ROTATION_DIR
    case "$doomed" in
        "$RUNTIME_DIR"/rotation.??????) ;;
        *) die "refusing to clean a path outside the active rotation pattern" ;;
    esac
    rm -rf -- "$doomed"
    rm -f -- "$ACTIVE_STATE"
    log "Removed protected rollback state"
}

case "${1:-}" in
    prepare) acquire_rotation_lock; phase_prepare ;;
    rotate-tunnel) acquire_rotation_lock; phase_rotate_tunnel ;;
    rotate-storage) acquire_rotation_lock; phase_rotate_storage ;;
    cleanup) acquire_rotation_lock; phase_cleanup ;;
    -h|--help) usage ;;
    *) usage >&2; exit 2 ;;
esac
