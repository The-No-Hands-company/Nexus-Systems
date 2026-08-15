#!/usr/bin/env bash
# Checkpointed production credential rotation. Secret values stay in mode-0600
# files or redirected stdin; command arguments and status output remain safe.

set -euo pipefail
set -E
umask 077

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNTIME_DIR="${NEXUS_ROTATION_RUNTIME_DIR:-/tmp/nexus-production}"
SECRETS_DIR="$RUNTIME_DIR/secrets"
ROOT_ENV="${NEXUS_ROTATION_ROOT_ENV:-$ROOT/.env}"
CLOUD_ENV="${NEXUS_ROTATION_CLOUD_ENV:-$ROOT/apps/Nexus-Cloud/.env}"
TUNNEL_TOKEN_FILE="${NEXUS_ROTATION_TUNNEL_TOKEN_FILE:-$SECRETS_DIR/cloudflared.token}"
ACTIVE_STATE="$RUNTIME_DIR/rotation.current"
CLOUDFLARED_COMPOSE="${NEXUS_ROTATION_CLOUDFLARED_COMPOSE:-$ROOT/deploy/production/cloudflared.compose.yml}"
INFRA_COMPOSE="${NEXUS_ROTATION_INFRA_COMPOSE:-$ROOT/docker-compose.yml}"
CF_API_BASE="${NEXUS_ROTATION_CF_API_BASE:-https://api.cloudflare.com/client/v4}"
MINIO_CONTAINER="${NEXUS_ROTATION_MINIO_CONTAINER:-nexus-systems-minio-1}"
MINIO_HEALTH_ATTEMPTS="${NEXUS_ROTATION_MINIO_HEALTH_ATTEMPTS:-30}"
MINIO_HEALTH_INTERVAL="${NEXUS_ROTATION_MINIO_HEALTH_INTERVAL:-2}"

ROTATION_DIR=
STORAGE_PENDING=0

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
  rotate-tunnel   Rotate the tunnel secret, install its token file, recreate
                  cloudflared, then disconnect previous tunnel connections.
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
}

on_error() {
    local status=$1
    local rollback_status=0

    trap - ERR
    set +e
    if [ "$STORAGE_PENDING" -eq 1 ]; then
        warn "storage rotation failed before health; restoring both environment files"
        rollback_storage
        rollback_status=$?
        if [ "$rollback_status" -eq 0 ]; then
            warn "paired storage rollback completed and MinIO was recreated"
        else
            warn "paired storage rollback requires immediate operator attention"
        fi
    fi
    exit "$status"
}
trap 'on_error $?' ERR

phase_prepare() {
    local rotation_dir state_source

    ensure_runtime_dirs
    require_file "$ROOT_ENV" "root environment file"
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"
    if [ -e "$ACTIVE_STATE" ]; then
        die "an active rotation already exists; verify it and run cleanup first"
    fi

    rotation_dir="$(mktemp -d "$RUNTIME_DIR/rotation.XXXXXX")"
    chmod 700 "$rotation_dir"
    ROTATION_DIR=$rotation_dir
    install -m 600 "$ROOT_ENV" "$ROTATION_DIR/root.env.rollback"
    install -m 600 "$CLOUD_ENV" "$ROTATION_DIR/cloud.env.rollback"
    if [ -f "$TUNNEL_TOKEN_FILE" ]; then
        install -m 600 "$TUNNEL_TOKEN_FILE" "$ROTATION_DIR/cloudflared.token.rollback"
    fi
    state_source="$ROTATION_DIR/active-path"
    printf '%s\n' "$ROTATION_DIR" > "$state_source"
    install -m 600 "$state_source" "$ACTIVE_STATE"
    log "Prepared protected rollback state in $ROTATION_DIR"
}

phase_rotate_tunnel() {
    local api_token zone_id tunnel_id auth_config zone_response account_id
    local tunnel_secret patch_body patch_response token_response staged_token

    load_rotation_dir
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"
    required_env_value_into api_token "${CF_API_TOKEN:-}" "$CLOUD_ENV" CF_API_TOKEN
    required_env_value_into zone_id "${CF_ZONE_ID:-}" "$CLOUD_ENV" CF_ZONE_ID
    required_env_value_into tunnel_id "${NEXUS_TUNNEL_ID:-}" "$CLOUD_ENV" NEXUS_TUNNEL_ID

    auth_config="$ROTATION_DIR/cloudflare.curl-config"
    write_cloudflare_auth_config "$api_token" "$auth_config"
    unset CF_API_TOKEN
    api_token=

    zone_response="$ROTATION_DIR/cloudflare-zone-response.json"
    cf_request GET "$CF_API_BASE/zones/$zone_id" "$zone_response"
    account_id="$(jq -er '.result.account.id | select(type == "string" and test("^[0-9A-Fa-f]{32}$"))' "$zone_response")"

    tunnel_secret="$ROTATION_DIR/tunnel-secret.new"
    patch_body="$ROTATION_DIR/tunnel-secret-patch.json"
    patch_response="$ROTATION_DIR/tunnel-patch-response.json"
    openssl rand -base64 32 > "$tunnel_secret"
    chmod 600 "$tunnel_secret"
    jq -Rs '{tunnel_secret: rtrimstr("\n")}' < "$tunnel_secret" > "$patch_body"
    chmod 600 "$patch_body"
    cf_request PATCH "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id" "$patch_response" "$patch_body"

    token_response="$ROTATION_DIR/tunnel-token-response.json"
    staged_token="$ROTATION_DIR/cloudflared.token.new"
    cf_request GET "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/token" "$token_response"
    jq -jer '.result | select(type == "string" and length > 0)' "$token_response" > "$staged_token"
    chmod 600 "$staged_token"
    install -d -m 700 "$(dirname "$TUNNEL_TOKEN_FILE")"
    install -m 600 "$staged_token" "$TUNNEL_TOKEN_FILE.next"
    mv -f -- "$TUNNEL_TOKEN_FILE.next" "$TUNNEL_TOKEN_FILE"

    # The pre-containment container may have been created with `docker run` and
    # therefore cannot be adopted by Compose. Remove only the exact tunnel
    # container name; Compose immediately recreates that one service.
    docker rm -f cloudflared >/dev/null 2>&1 || true
    CLOUDFLARED_TOKEN_FILE="$TUNNEL_TOKEN_FILE" \
        docker compose -f "$CLOUDFLARED_COMPOSE" up -d --force-recreate cloudflared
    cf_request DELETE "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/connections" \
        "$ROTATION_DIR/tunnel-connections-delete-response.json"
    install -m 600 /dev/null "$ROTATION_DIR/tunnel.checkpoint"
    log "Tunnel rotation reached the token-file and connection-cleanup checkpoint"
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
    prepare) phase_prepare ;;
    rotate-tunnel) phase_rotate_tunnel ;;
    rotate-storage) phase_rotate_storage ;;
    cleanup) phase_cleanup ;;
    -h|--help) usage ;;
    *) usage >&2; exit 2 ;;
esac
