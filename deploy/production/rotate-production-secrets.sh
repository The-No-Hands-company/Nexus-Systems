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
CHAT_ENV="${NEXUS_ROTATION_CHAT_ENV:-$ROOT/deploy/production/nexus-chat.env}"
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
HTTP_CONNECT_TIMEOUT="${NEXUS_ROTATION_HTTP_CONNECT_TIMEOUT:-5}"
HTTP_TOTAL_TIMEOUT="${NEXUS_ROTATION_HTTP_TOTAL_TIMEOUT:-20}"
STORAGE_LOCK_TIMEOUT="${NEXUS_ROTATION_STORAGE_LOCK_TIMEOUT:-10}"

ROTATION_DIR=
STORAGE_PENDING=0
TUNNEL_RECOVERY_PENDING=0
TUNNEL_STAGED_TOKEN=
TUNNEL_ACCOUNT_ID=
TUNNEL_ID=
TUNNEL_OLD_CONNECTION_IDS=
EXIT_GUARD=0
ROTATION_LOCK_FD=
STORAGE_ENV_LOCKS_HELD=0
STORAGE_ENV_LOCK_FDS=()

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
  prepare         Back up root, Cloud, deployed Chat, and the current tunnel
                  token in protected state. If needed, bootstrap the token from
                  the running connector's protected argv inspection.
  rotate-tunnel   Adopt the token produced by an operator's completed dashboard
                  rotation, verify its connector, then disconnect old clients.
  rotate-storage  Replace the atomic MinIO/Cloud/deployed-Chat credential set,
                  recreate MinIO, and restore keys if health is missed.
  rollback-storage
                  Restore only the prepared credential keys after a consumer
                  restart or authenticated S3 failure, preserving other edits.
  with-storage-locks COMMAND [ARG ...]
                  Run a repository-owned/operator environment edit while
                  holding the same stable root, Cloud, and Chat writer locks.
  cleanup         Remove the active protected rollback directory after the
                  tunnel and storage checkpoints are both complete.

All repository-owned or operator edits to the storage credential environment
files during an active rotation must use with-storage-locks.
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

acquire_storage_environment_locks() {
    local env_file lock_file lock_fd
    local -a env_files=("$ROOT_ENV" "$CLOUD_ENV")

    [ "$STORAGE_ENV_LOCKS_HELD" -eq 0 ] || return 0
    if { [ -n "$ROTATION_DIR" ] && [ -f "$ROTATION_DIR/chat.env.rollback" ]; } \
        || [ -e "$CHAT_ENV" ]; then
        env_files+=("$CHAT_ENV")
    fi
    # These stable, adjacent lockfiles are the writer protocol for every
    # repository-owned rotation of this credential set. Holding them across
    # snapshot, comparison, rename, compensation, and MinIO health closes the
    # compare/rename window without locking an inode that rename replaces.
    for env_file in "${env_files[@]}"; do
        lock_file="$env_file.nexus-storage.lock"
        if ! exec {lock_fd}>"$lock_file"; then
            warn "could not open the stable storage environment lock for $(basename "$env_file")"
            return 1
        fi
        if ! chmod 600 "$lock_file"; then
            warn "could not protect the stable storage environment lock for $(basename "$env_file")"
            return 1
        fi
        if ! flock -w "$STORAGE_LOCK_TIMEOUT" "$lock_fd"; then
            warn "timed out waiting for the stable storage environment lock for $(basename "$env_file")"
            return 1
        fi
        STORAGE_ENV_LOCK_FDS+=("$lock_fd")
    done
    STORAGE_ENV_LOCKS_HELD=1
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

read_unique_env_value_into() {
    local target=$1
    local file=$2
    local key=$3
    local line extracted= matches=0

    while IFS= read -r line || [ -n "$line" ]; do
        line=${line%$'\r'}
        case "$line" in
            "$key="*)
                matches=$((matches + 1))
                extracted=${line#*=}
                if [[ "$extracted" == \"*\" ]] && [ "${#extracted}" -ge 2 ]; then
                    extracted=${extracted:1:${#extracted}-2}
                elif [[ "$extracted" == \'*\' ]] && [ "${#extracted}" -ge 2 ]; then
                    extracted=${extracted:1:${#extracted}-2}
                fi
                ;;
        esac
    done < "$file"
    [ "$matches" -eq 1 ] || {
        [ "$matches" -eq 0 ] && return 1
        return 2
    }
    printf -v "$target" '%s' "$extracted"
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

require_env_value_present() {
    local file=$1
    local key=$2
    local label=$3
    local value=

    read_env_value_into value "$file" "$key" || true
    [ -n "$value" ] || die "$label must contain a non-empty $key before rotation"
}

require_unique_env_value_present() {
    local file=$1
    local key=$2
    local label=$3
    local value= status

    if read_unique_env_value_into value "$file" "$key"; then
        [ -n "$value" ] || die "$label must contain a non-empty $key before rotation"
        return 0
    else
        status=$?
    fi
    if [ "$status" -eq 2 ]; then
        die "$label contains duplicate $key assignments; refusing ambiguous rotation"
    fi
    die "$label must contain exactly one non-empty $key before rotation"
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
        --connect-timeout "$HTTP_CONNECT_TIMEOUT"
        --max-time "$HTTP_TOTAL_TIMEOUT"
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
    curl --silent --show-error --fail \
        --connect-timeout "$HTTP_CONNECT_TIMEOUT" \
        --max-time "$HTTP_TOTAL_TIMEOUT" \
        --output "$output" "$url"
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

stage_rotated_env_file() {
    local staged_target=$1
    local snapshot_target=$2
    local current_file=$3
    local backup_file=$4
    local label=$5
    local env_dir env_base snapshot staged key expected_new_file
    local current_value old_value expected_new_value line matched index read_status
    local staged_value
    local -a keys=()
    local -a old_values=()

    shift 5
    [ -f "$current_file" ] || {
        warn "$label environment file is missing"
        return 1
    }
    [ -f "$backup_file" ] || {
        warn "prepared $label rollback is missing"
        return 1
    }
    [ "$#" -gt 0 ] && [ $(( $# % 2 )) -eq 0 ] || {
        warn "$label rollback key set is invalid"
        return 1
    }

    env_dir="$(dirname "$current_file")"
    env_base="$(basename "$current_file")"
    if ! snapshot="$(mktemp "$env_dir/.${env_base}.rollback-source.XXXXXX")"; then
        warn "$label could not create its protected rollback snapshot"
        return 1
    fi
    if ! staged="$(mktemp "$env_dir/.${env_base}.rollback-next.XXXXXX")"; then
        rm -f -- "$snapshot"
        warn "$label could not create its protected staged rollback"
        return 1
    fi
    if ! install -m 600 "$current_file" "$snapshot"; then
        rm -f -- "$snapshot" "$staged"
        warn "$label could not be snapshotted for rollback"
        return 1
    fi

    while [ "$#" -gt 0 ]; do
        key=$1
        expected_new_file=$2
        shift 2
        current_value=
        old_value=
        expected_new_value=
        if read_unique_env_value_into current_value "$snapshot" "$key"; then
            :
        else
            read_status=$?
            if [ "$read_status" -eq 2 ]; then
                warn "$label contains duplicate $key assignments; refusing ambiguous rollback"
            else
                warn "$label no longer contains $key; refusing to overwrite a concurrent credential edit"
            fi
            rm -f -- "$snapshot" "$staged"
            return 1
        fi
        if read_unique_env_value_into old_value "$backup_file" "$key"; then
            :
        else
            read_status=$?
            if [ "$read_status" -eq 2 ]; then
                warn "prepared $label rollback contains duplicate $key assignments"
            else
                warn "prepared $label rollback does not contain $key"
            fi
            rm -f -- "$snapshot" "$staged"
            return 1
        fi
        if [ ! -f "$expected_new_file" ]; then
            warn "generated replacement for $label $key is unavailable"
            rm -f -- "$snapshot" "$staged"
            return 1
        fi
        IFS= read -r expected_new_value < "$expected_new_file" || true
        [ -n "$expected_new_value" ] || {
            warn "generated replacement for $label $key is unavailable"
            rm -f -- "$snapshot" "$staged"
            return 1
        }
        if [ "$current_value" != "$old_value" ] && [ "$current_value" != "$expected_new_value" ]; then
            warn "$label $key changed outside this rotation; refusing to discard the concurrent value"
            rm -f -- "$snapshot" "$staged"
            return 1
        fi
        keys+=("$key")
        old_values+=("$old_value")
    done

    if ! (
        while IFS= read -r line || [ -n "$line" ]; do
            matched=0
            for ((index = 0; index < ${#keys[@]}; index++)); do
                case "$line" in
                    "${keys[$index]}="*)
                        printf '%s=%s\n' "${keys[$index]}" "${old_values[$index]}" || exit 1
                        matched=1
                        break
                        ;;
                esac
            done
            if [ "$matched" -eq 0 ]; then
                printf '%s\n' "$line" || exit 1
            fi
        done < "$snapshot"
    ) > "$staged"; then
        rm -f -- "$snapshot" "$staged"
        warn "$label could not render its complete staged rollback"
        return 1
    fi
    if ! chmod 600 "$staged"; then
        rm -f -- "$snapshot" "$staged"
        warn "$label could not protect its staged rollback"
        return 1
    fi
    for ((index = 0; index < ${#keys[@]}; index++)); do
        staged_value=
        if ! read_unique_env_value_into staged_value "$staged" "${keys[$index]}" \
            || [ "$staged_value" != "${old_values[$index]}" ]; then
            rm -f -- "$snapshot" "$staged"
            warn "$label staged rollback failed its complete credential post-condition"
            return 1
        fi
    done
    if ! printf -v "$staged_target" '%s' "$staged" \
        || ! printf -v "$snapshot_target" '%s' "$snapshot"; then
        rm -f -- "$snapshot" "$staged"
        warn "$label could not retain its protected rollback artifact paths"
        return 1
    fi
}

remove_storage_rollback_artifacts() {
    local path

    for path in "$@"; do
        [ -z "$path" ] || rm -f -- "$path"
    done
}

restore_storage_rollback_snapshot() {
    local snapshot=$1
    local target=$2
    local label=$3

    if mv -f -- "$snapshot" "$target"; then
        return 0
    fi
    warn "rollback compensation failed for $label; protected recovery snapshot retained at $snapshot"
    return 1
}

rollback_storage() {
    local new_user="$ROTATION_DIR/minio-root-user.new"
    local new_password="$ROTATION_DIR/minio-root-password.new"
    local root_staged= root_snapshot= cloud_staged= cloud_snapshot=
    local chat_staged= chat_snapshot= root_committed=0 cloud_committed=0
    local compensation_failed=0

    if [ -f "$ROTATION_DIR/chat.env.rollback" ] && [ ! -f "$CHAT_ENV" ]; then
        warn "Nexus Chat environment disappeared after prepare; paired rollback requires operator attention"
        warn "atomic credential-key rollback could not be completed"
        return 1
    fi

    if ! stage_rotated_env_file root_staged root_snapshot \
        "$ROOT_ENV" "$ROTATION_DIR/root.env.rollback" root-env \
        MINIO_ROOT_USER "$new_user" \
        MINIO_ROOT_PASSWORD "$new_password"; then
        warn "atomic credential-key rollback could not be completed"
        return 1
    fi
    if ! stage_rotated_env_file cloud_staged cloud_snapshot \
        "$CLOUD_ENV" "$ROTATION_DIR/cloud.env.rollback" cloud-env \
        NEXUS_STORAGE_S3_ACCESS_KEY "$new_user" \
        NEXUS_STORAGE_S3_SECRET_KEY "$new_password"; then
        remove_storage_rollback_artifacts "$root_staged" "$root_snapshot"
        warn "atomic credential-key rollback could not be completed"
        return 1
    fi
    if [ -f "$ROTATION_DIR/chat.env.rollback" ]; then
        if ! stage_rotated_env_file chat_staged chat_snapshot \
            "$CHAT_ENV" "$ROTATION_DIR/chat.env.rollback" chat-env \
            NEXUS__STORAGE__ACCESS_KEY "$new_user" \
            NEXUS__STORAGE__SECRET_KEY "$new_password"; then
            remove_storage_rollback_artifacts \
                "$root_staged" "$root_snapshot" "$cloud_staged" "$cloud_snapshot"
            warn "atomic credential-key rollback could not be completed"
            return 1
        fi
    fi

    if ! cmp -s "$root_snapshot" "$ROOT_ENV" \
        || ! cmp -s "$cloud_snapshot" "$CLOUD_ENV" \
        || { [ -n "$chat_snapshot" ] && ! cmp -s "$chat_snapshot" "$CHAT_ENV"; }; then
        remove_storage_rollback_artifacts \
            "$root_staged" "$root_snapshot" "$cloud_staged" "$cloud_snapshot" \
            "$chat_staged" "$chat_snapshot"
        warn "an environment file changed while rollback was staged; refusing to discard the concurrent edit"
        return 1
    fi

    if ! mv -f -- "$root_staged" "$ROOT_ENV"; then
        remove_storage_rollback_artifacts \
            "$root_staged" "$root_snapshot" "$cloud_staged" "$cloud_snapshot" \
            "$chat_staged" "$chat_snapshot"
        warn "atomic credential-key rollback could not install the root environment"
        return 1
    fi
    root_committed=1
    if ! mv -f -- "$cloud_staged" "$CLOUD_ENV"; then
        if ! restore_storage_rollback_snapshot "$root_snapshot" "$ROOT_ENV" root-env; then
            warn "rollback commit failed and compensation failed; all remaining protected artifacts were retained"
            return 1
        fi
        remove_storage_rollback_artifacts \
            "$cloud_staged" "$cloud_snapshot" "$chat_staged" "$chat_snapshot"
        warn "atomic credential-key rollback could not install the Cloud environment"
        return 1
    fi
    cloud_committed=1
    if [ -n "$chat_staged" ] && ! mv -f -- "$chat_staged" "$CHAT_ENV"; then
        if [ "$cloud_committed" -ne 0 ] \
            && ! restore_storage_rollback_snapshot "$cloud_snapshot" "$CLOUD_ENV" cloud-env; then
            compensation_failed=1
        fi
        if [ "$root_committed" -ne 0 ] \
            && ! restore_storage_rollback_snapshot "$root_snapshot" "$ROOT_ENV" root-env; then
            compensation_failed=1
        fi
        if [ "$compensation_failed" -ne 0 ]; then
            warn "rollback commit failed and compensation failed; all remaining protected artifacts were retained"
            return 1
        fi
        remove_storage_rollback_artifacts "$chat_staged" "$chat_snapshot"
        warn "atomic credential-key rollback could not install the Nexus Chat environment"
        return 1
    fi
    remove_storage_rollback_artifacts "$root_snapshot" "$cloud_snapshot" "$chat_snapshot"

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
        warn "storage rotation stopped before health; restoring the atomic credential-key set"
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
    local argv_snapshot bootstrap_token

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
    if [ -f "$CHAT_ENV" ]; then
        install -m 600 "$CHAT_ENV" "$ROTATION_DIR/chat.env.rollback"
    fi
    if [ ! -f "$TUNNEL_TOKEN_FILE" ]; then
        argv_snapshot="$ROTATION_DIR/cloudflared.argv.json"
        bootstrap_token="$ROTATION_DIR/cloudflared.token.bootstrap"
        install -m 600 /dev/null "$argv_snapshot"
        docker inspect --format '{{json .Config.Cmd}}' cloudflared > "$argv_snapshot" || \
            die "current tunnel token file is missing and cloudflared argv could not be inspected"
        install -m 600 /dev/null "$bootstrap_token"
        jq -jer '
            if type != "array" then error("cloudflared argv is not an array") else . end
            | [
                range(0; length) as $index
                | if .[$index] == "--token" then .[$index + 1]
                  elif ((.[$index] | type) == "string" and (.[$index] | startswith("--token=")))
                  then (.[$index] | sub("^--token="; ""))
                  else empty
                  end
              ] as $tokens
            | if (($tokens | length) == 1
                  and ($tokens[0] | type) == "string"
                  and ($tokens[0] | length) > 0)
              then $tokens[0]
              else error("cloudflared argv must contain exactly one non-empty token")
              end
        ' "$argv_snapshot" > "$bootstrap_token" || \
            die "current tunnel token could not be isolated from protected cloudflared argv"
        chmod 600 "$bootstrap_token"
        install_tunnel_token "$bootstrap_token"
        rm -f -- "$argv_snapshot" "$bootstrap_token"
    fi
    require_file "$TUNNEL_TOKEN_FILE" "current tunnel token file"
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
    log "Next: rotate the tunnel token explicitly in the Cloudflare dashboard, then run rotate-tunnel"
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
    local post_token_response post_token post_hash installed_hash

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

    post_token_response="$ROTATION_DIR/tunnel-token-post-verification.json"
    post_token="$ROTATION_DIR/cloudflared.token.post-verification"
    cf_request GET "$CF_API_BASE/accounts/$account_id/cfd_tunnel/$tunnel_id/token" "$post_token_response"
    jq -jer '.result | select(type == "string" and length > 0)' "$post_token_response" > "$post_token"
    chmod 600 "$post_token"
    post_hash="$(sha256sum "$post_token" | awk '{print $1}')"
    installed_hash="$(sha256sum "$TUNNEL_TOKEN_FILE" | awk '{print $1}')"
    [ "$post_hash" = "$new_hash" ] && [ "$installed_hash" = "$new_hash" ] && [ "$new_hash" != "$old_hash" ] || \
        die "adopted tunnel token failed the dashboard GET and installed-file hash post-condition"
    printf '%s\n' "$installed_hash" > "$ROTATION_DIR/cloudflared.token.adopted.sha256"
    chmod 600 "$ROTATION_DIR/cloudflared.token.adopted.sha256"

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
    if [ -f "$ROTATION_DIR/chat.env.rollback" ]; then
        require_file "$CHAT_ENV" "Nexus Chat environment file"
    elif [ -e "$CHAT_ENV" ]; then
        die "Nexus Chat environment appeared after prepare; restart prepare so it joins the atomic credential set"
    fi
    acquire_storage_environment_locks || die "storage credential files are being updated by another writer"
    require_unique_env_value_present "$ROOT_ENV" MINIO_ROOT_USER "root environment"
    require_unique_env_value_present "$ROOT_ENV" MINIO_ROOT_PASSWORD "root environment"
    require_unique_env_value_present "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY "Nexus Cloud environment"
    require_unique_env_value_present "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY "Nexus Cloud environment"
    require_unique_env_value_present "$ROTATION_DIR/root.env.rollback" MINIO_ROOT_USER \
        "prepared root rollback"
    require_unique_env_value_present "$ROTATION_DIR/root.env.rollback" MINIO_ROOT_PASSWORD \
        "prepared root rollback"
    require_unique_env_value_present "$ROTATION_DIR/cloud.env.rollback" NEXUS_STORAGE_S3_ACCESS_KEY \
        "prepared Cloud rollback"
    require_unique_env_value_present "$ROTATION_DIR/cloud.env.rollback" NEXUS_STORAGE_S3_SECRET_KEY \
        "prepared Cloud rollback"
    if [ -f "$ROTATION_DIR/chat.env.rollback" ]; then
        require_unique_env_value_present "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY "Nexus Chat environment"
        require_unique_env_value_present "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY "Nexus Chat environment"
        require_unique_env_value_present "$ROTATION_DIR/chat.env.rollback" NEXUS__STORAGE__ACCESS_KEY \
            "prepared Nexus Chat rollback"
        require_unique_env_value_present "$ROTATION_DIR/chat.env.rollback" NEXUS__STORAGE__SECRET_KEY \
            "prepared Nexus Chat rollback"
    fi

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
    if [ -f "$ROTATION_DIR/chat.env.rollback" ]; then
        replace_env_value_from_file "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY "$new_user"
        replace_env_value_from_file "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY "$new_password"
    fi
    recreate_minio
    wait_for_minio_health
    STORAGE_PENDING=0
    install -m 600 /dev/null "$ROTATION_DIR/storage.checkpoint"
    log "Storage rotation reached the paired MinIO health checkpoint"
}

phase_rollback_storage() {
    load_rotation_dir
    require_file "$ROTATION_DIR/root.env.rollback" "root environment rollback"
    require_file "$ROTATION_DIR/cloud.env.rollback" "Cloud environment rollback"
    require_file "$ROTATION_DIR/storage.checkpoint" "completed storage checkpoint"
    acquire_storage_environment_locks || die "storage credential files are being updated by another writer"

    STORAGE_PENDING=1
    rollback_storage
    STORAGE_PENDING=0
    rm -f -- "$ROTATION_DIR/storage.checkpoint"
    log "Restored the prepared storage credential pair and removed the storage checkpoint"
}

phase_cleanup() {
    local doomed

    load_rotation_dir
    require_file "$ROTATION_DIR/tunnel.checkpoint" "completed tunnel checkpoint"
    require_file "$ROTATION_DIR/storage.checkpoint" "completed storage checkpoint"
    doomed=$ROTATION_DIR
    case "$doomed" in
        "$RUNTIME_DIR"/rotation.??????) ;;
        *) die "refusing to clean a path outside the active rotation pattern" ;;
    esac
    rm -rf -- "$doomed"
    rm -f -- "$ACTIVE_STATE"
    log "Removed protected rollback state"
}

phase_with_storage_locks() {
    [ "$#" -gt 0 ] || die "with-storage-locks requires a command"
    [ "${1:-}" != -- ] || shift
    [ "$#" -gt 0 ] || die "with-storage-locks requires a command"
    require_file "$ROOT_ENV" "root environment file"
    require_file "$CLOUD_ENV" "Nexus Cloud environment file"
    acquire_storage_environment_locks || die "storage credential files are being updated by another writer"
    "$@"
}

case "${1:-}" in
    prepare) acquire_rotation_lock; phase_prepare ;;
    rotate-tunnel) acquire_rotation_lock; phase_rotate_tunnel ;;
    rotate-storage) acquire_rotation_lock; phase_rotate_storage ;;
    rollback-storage) acquire_rotation_lock; phase_rollback_storage ;;
    with-storage-locks) shift; phase_with_storage_locks "$@" ;;
    cleanup) acquire_rotation_lock; phase_cleanup ;;
    -h|--help) usage ;;
    *) usage >&2; exit 2 ;;
esac
