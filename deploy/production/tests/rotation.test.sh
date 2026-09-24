#!/usr/bin/env bash

set -euo pipefail
umask 077

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SCRIPT="$ROOT/deploy/production/rotate-production-secrets.sh"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-rotation-test.XXXXXX")"
trap 'rm -rf -- "$TEST_ROOT"' EXIT

passes=0

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

[ -f "$SCRIPT" ] || fail "rotation script is missing: $SCRIPT"

assert_eq() {
    local want=$1
    local got=$2
    local message=$3
    [ "$got" = "$want" ] || fail "$message (want: $want, got: $got)"
}

assert_file_contains() {
    local file=$1
    local needle=$2
    local message=$3
    grep -Fq -- "$needle" "$file" || fail "$message"
}

assert_file_not_contains() {
    local file=$1
    local needle=$2
    local message=$3
    if grep -Fq -- "$needle" "$file"; then
        fail "$message"
    fi
}

assert_text_contains() {
    local text=$1
    local needle=$2
    local message=$3
    [[ "$text" == *"$needle"* ]] || fail "$message"
}

assert_file_order() {
    local file=$1
    local first=$2
    local second=$3
    local message=$4
    local first_line second_line

    first_line="$(grep -nF -- "$first" "$file" | head -1 | cut -d: -f1 || true)"
    second_line="$(grep -nF -- "$second" "$file" | head -1 | cut -d: -f1 || true)"
    [ -n "$first_line" ] && [ -n "$second_line" ] && [ "$first_line" -lt "$second_line" ] || fail "$message"
}

count_matches() {
    local file=$1
    local needle=$2
    [ -f "$file" ] || { printf '0'; return; }
    grep -Fc -- "$needle" "$file" || true
}

env_value() {
    local file=$1
    local key=$2
    local line

    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            "$key="*)
                printf '%s' "${line#*=}"
                return 0
                ;;
        esac
    done < "$file"
    return 1
}

env_effective_value() {
    local file=$1
    local key=$2
    local line value= found=0

    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            "$key="*) value=${line#*=}; found=1 ;;
        esac
    done < "$file"
    [ "$found" -eq 1 ] || return 1
    printf '%s' "$value"
}

bash_effective_env_value() {
    local file=$1
    local key=$2

    bash -c '
        set -euo pipefail
        unset "$2"
        set -a
        # shellcheck source=/dev/null
        . "$1"
        set +a
        printf "%s" "${!2-}"
    ' bash "$file" "$key"
}

file_mode() {
    stat -c '%a' "$1"
}

write_fake_commands() {
    local case_dir=$1
    local fake_bin="$case_dir/fake-bin"

    mkdir -p "$fake_bin"

    cat > "$fake_bin/install" <<'FAKE_INSTALL'
#!/usr/bin/env bash
set -euo pipefail
umask 077
[ "${CF_API_TOKEN+x}" != x ] || { printf 'install inherited CF_API_TOKEN\n' >&2; exit 97; }
{
    printf 'install'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/install.argv"

if [ -n "${FAKE_INSTALL_FAIL_ON:-}" ] && [[ " $* " == *"$FAKE_INSTALL_FAIL_ON"* ]]; then
    marker="$FAKE_RECORD_DIR/install-fail-once"
    if [ ! -e "$marker" ]; then
        : > "$marker"
        printf 'simulated install failure\n' >&2
        exit 1
    fi
fi

/usr/bin/install "$@"

destination=${*: -1}
case "$destination" in
    *.rotation.install)
        counter_file="$FAKE_RECORD_DIR/env-replacement.count"
        count=0
        [ ! -f "$counter_file" ] || read -r count < "$counter_file"
        count=$((count + 1))
        printf '%s\n' "$count" > "$counter_file"
        if [ "${FAKE_INSTALL_FAIL_ENV_REPLACE_NUMBER:-}" = "$count" ]; then
            printf 'simulated environment replacement failure %s\n' "$count" >&2
            exit 1
        fi
        if [ "${FAKE_INSTALL_SIGNAL_ENV_REPLACE_NUMBER:-}" = "$count" ]; then
            kill -s "${FAKE_INSTALL_SIGNAL:-TERM}" "$PPID"
        fi
        ;;
esac

if [ -n "${FAKE_INSTALL_BLOCK_ON:-}" ] && [[ " $* " == *"$FAKE_INSTALL_BLOCK_ON"* ]]; then
    : > "$FAKE_INSTALL_BLOCK_READY_FILE"
    for ((attempt = 1; attempt <= 100; attempt++)); do
        [ ! -e "$FAKE_INSTALL_BLOCK_RELEASE_FILE" ] || exit 0
        sleep 0.05
    done
    printf 'timed out waiting to release fake install\n' >&2
    exit 2
fi
FAKE_INSTALL

    cat > "$fake_bin/openssl" <<'FAKE_OPENSSL'
#!/usr/bin/env bash
set -euo pipefail
umask 077
[ "${CF_API_TOKEN+x}" != x ] || { printf 'openssl inherited CF_API_TOKEN\n' >&2; exit 97; }
{
    printf 'openssl'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/openssl.argv"

case " $* " in
    *' -hex '*) input=$FAKE_OPENSSL_HEX_FILE ;;
    *' -base64 '*) input=$FAKE_OPENSSL_BASE64_FILE ;;
    *) printf 'unsupported fake openssl invocation\n' >&2; exit 2 ;;
esac
[ -f "$input" ] || { printf 'fake openssl input is missing\n' >&2; exit 2; }
[ "$(stat -c '%a' "$input")" = 600 ] || { printf 'fake openssl input is not private\n' >&2; exit 2; }
/bin/cat "$input"
FAKE_OPENSSL

    cat > "$fake_bin/curl" <<'FAKE_CURL'
#!/usr/bin/env bash
set -euo pipefail
umask 077
[ "${CF_API_TOKEN+x}" != x ] || { printf 'curl inherited CF_API_TOKEN\n' >&2; exit 97; }
{
    printf 'curl'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/curl.argv"

method=GET
output=
url=
config=
data_file=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --request|-X)
            method=$2
            shift 2
            ;;
        --output|-o)
            output=$2
            shift 2
            ;;
        --config|-K)
            config=$2
            shift 2
            ;;
        --header|-H)
            shift 2
            ;;
        --data-binary)
            data_file=$2
            shift 2
            ;;
        --silent|--show-error|--fail|--fail-with-body)
            shift
            ;;
        http://*|https://*)
            url=$1
            shift
            ;;
        *)
            shift
            ;;
    esac
done

[ -n "$output" ] || { printf 'fake curl requires --output\n' >&2; exit 2; }
[ -n "$url" ] || { printf 'fake curl requires a URL\n' >&2; exit 2; }
printf 'curl %s %s\n' "$method" "$url" >> "$FAKE_RECORD_DIR/events"
[ -f "$output" ] || { printf 'fake curl output was not pre-created\n' >&2; exit 2; }
[ "$(stat -c '%a' "$output")" = 600 ] || { printf 'fake curl output is not private\n' >&2; exit 2; }

case "$url" in
    "$FAKE_CF_API_BASE"/*)
        [ -n "$config" ] || { printf 'Cloudflare request has no curl config\n' >&2; exit 2; }
        [ -f "$config" ] || { printf 'curl config is missing\n' >&2; exit 2; }
        [ "$(stat -c '%a' "$config")" = 600 ] || { printf 'curl config is not private\n' >&2; exit 2; }
        cmp -s "$FAKE_EXPECTED_CURL_CONFIG_FILE" "$config" || { printf 'curl config has wrong authentication\n' >&2; exit 2; }
        ;;
    *)
        [ -z "$config" ] || { printf 'public request unexpectedly used auth config\n' >&2; exit 2; }
        ;;
esac

if [ -n "$data_file" ]; then
    case "$data_file" in
        @*) data_file=${data_file#@} ;;
        *) printf 'curl data must come from a file\n' >&2; exit 2 ;;
    esac
    [ -f "$data_file" ] || { printf 'curl data file is missing\n' >&2; exit 2; }
    [ "$(stat -c '%a' "$data_file")" = 600 ] || { printf 'curl data file is not private\n' >&2; exit 2; }
    sha256sum "$data_file" >> "$FAKE_RECORD_DIR/curl-data.sha256"
fi

if [ -n "${FAKE_CURL_FAIL_ON:-}" ] && [[ "$method $url" == *"$FAKE_CURL_FAIL_ON"* ]]; then
    marker="$FAKE_RECORD_DIR/curl-fail-once"
    if [ ! -e "$marker" ]; then
        : > "$marker"
        printf 'simulated curl failure\n' >&2
        exit 22
    fi
fi

case "$method $url" in
    "GET "*'/zones/'*)
        printf '{"success":true,"result":{"account":{"id":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}}}\n' > "$output"
        ;;
    "GET "*'/connections')
        counter_file="$FAKE_RECORD_DIR/connection-get.count"
        count=0
        [ ! -f "$counter_file" ] || read -r count < "$counter_file"
        count=$((count + 1))
        printf '%s\n' "$count" > "$counter_file"
        if [ "$count" -eq 1 ]; then
            /bin/cat "$FAKE_OLD_CONNECTIONS_RESPONSE_FILE" > "$output"
        else
            /bin/cat "$FAKE_NEW_CONNECTIONS_RESPONSE_FILE" > "$output"
        fi
        ;;
    "GET "*'/token')
        /bin/cat "$FAKE_TUNNEL_TOKEN_RESPONSE_FILE" > "$output"
        ;;
    "DELETE "*'/connections?client_id='*)
        printf '{"success":true,"result":{}}\n' > "$output"
        ;;
    "GET "https://cloud.example.test/health)
        printf 'ok\n' > "$output"
        ;;
    *)
        printf 'unexpected fake curl request: %s %s\n' "$method" "$url" >&2
        exit 2
        ;;
esac
FAKE_CURL

    cat > "$fake_bin/docker" <<'FAKE_DOCKER'
#!/usr/bin/env bash
set -euo pipefail
umask 077
[ "${CF_API_TOKEN+x}" != x ] || { printf 'docker inherited CF_API_TOKEN\n' >&2; exit 97; }
{
    printf 'docker'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/docker.argv"
printf 'docker %s\n' "$*" >> "$FAKE_RECORD_DIR/events"

env_hash() {
    local file=$1
    local key=$2
    local line value

    [ -f "$file" ] || { printf 'absent'; return; }
    value=
    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            "$key="*) value=${line#*=}; break ;;
        esac
    done < "$file"
    printf '%s' "$value" | sha256sum | awk '{print $1}'
}

if [ "${1:-}" = inspect ]; then
    if [[ " $* " == *'.Config.Cmd'* ]]; then
        printf '%s\n' "${FAKE_TUNNEL_CMD_JSON:-[\"tunnel\",\"--no-autoupdate\",\"run\",\"--token\",\"OLD_TUNNEL_TOKEN_SENTINEL\"]}"
        exit 0
    fi
    case "${*: -1}" in
        cloudflared) printf '%s\n' "${FAKE_TUNNEL_RUNNING:-true}" ;;
        *)
            if [ -n "${FAKE_MINIO_HEALTH_SEQUENCE_FILE:-}" ]; then
                counter_file="$FAKE_RECORD_DIR/minio-inspect.count"
                count=0
                [ ! -f "$counter_file" ] || read -r count < "$counter_file"
                count=$((count + 1))
                printf '%s\n' "$count" > "$counter_file"
                status="$(sed -n "${count}p" "$FAKE_MINIO_HEALTH_SEQUENCE_FILE")"
                [ -n "$status" ] || status="$(tail -1 "$FAKE_MINIO_HEALTH_SEQUENCE_FILE")"
                printf '%s\n' "$status"
            else
                printf '%s\n' "${FAKE_MINIO_HEALTH:-healthy}"
            fi
            ;;
    esac
    exit 0
fi

if [[ " $* " == *' up '* ]] && [[ " $* " == *' minio '* ]]; then
    counter_file="$FAKE_RECORD_DIR/minio-up.count"
    count=0
    [ ! -f "$counter_file" ] || read -r count < "$counter_file"
    count=$((count + 1))
    printf '%s\n' "$count" > "$counter_file"
    printf 'minio-up %s root_user=%s root_password=%s cloud_access=%s cloud_secret=%s chat_access=%s chat_secret=%s\n' \
        "$count" \
        "$(env_hash "$ROTATION_ROOT_ENV" MINIO_ROOT_USER)" \
        "$(env_hash "$ROTATION_ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        "$(env_hash "$ROTATION_CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        "$(env_hash "$ROTATION_CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        "$(env_hash "$ROTATION_CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        "$(env_hash "$ROTATION_CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)" \
        >> "$FAKE_RECORD_DIR/minio-snapshots"
fi

if [ -n "${FAKE_DOCKER_FAIL_ON:-}" ] && [[ " $* " == *"$FAKE_DOCKER_FAIL_ON"* ]]; then
    marker="$FAKE_RECORD_DIR/docker-fail-once"
    if [ ! -e "$marker" ]; then
        : > "$marker"
        printf 'simulated docker failure\n' >&2
        exit 1
    fi
fi
FAKE_DOCKER

    cat > "$fake_bin/chmod" <<'FAKE_CHMOD'
#!/usr/bin/env bash
set -euo pipefail
if [ -n "${FAKE_CHMOD_FAIL_ON:-}" ] && [[ " $* " == *"$FAKE_CHMOD_FAIL_ON"* ]]; then
    printf 'simulated chmod failure\n' >&2
    exit 1
fi
/bin/chmod "$@"
FAKE_CHMOD

    cat > "$fake_bin/mv" <<'FAKE_MV'
#!/usr/bin/env bash
set -euo pipefail
joined=" $* "
if [ -n "${FAKE_MV_FAIL_ALWAYS_MATCH:-}" ] \
    && [[ "$joined" == *"$FAKE_MV_FAIL_ALWAYS_MATCH"* ]]; then
    printf 'simulated persistent mv failure\n' >&2
    exit 1
fi
if [ -n "${FAKE_MV_FAIL_ALWAYS_MATCH_2:-}" ] \
    && [[ "$joined" == *"$FAKE_MV_FAIL_ALWAYS_MATCH_2"* ]]; then
    printf 'simulated second persistent mv failure\n' >&2
    exit 1
fi
if [ -n "${FAKE_MV_FAIL_ONCE_MATCH:-}" ] \
    && [[ "$joined" == *"$FAKE_MV_FAIL_ONCE_MATCH"* ]]; then
    marker="$FAKE_RECORD_DIR/mv-fail-once"
    if [ ! -e "$marker" ]; then
        : > "$marker"
        printf 'simulated one-shot mv failure\n' >&2
        exit 1
    fi
fi
/usr/bin/mv "$@"
FAKE_MV

    cat > "$fake_bin/storage-env-writer" <<'FAKE_WRITER'
#!/usr/bin/env bash
set -euo pipefail
: > "$FAKE_WRITER_READY_FILE"
for ((attempt = 1; attempt <= 100; attempt++)); do
    if [ -e "$FAKE_WRITER_RELEASE_FILE" ]; then
        printf 'COOPERATIVE_LATE_EDIT=preserved\n' >> "$ROTATION_ROOT_ENV"
        exit 0
    fi
    sleep 0.05
done
printf 'timed out waiting to release cooperative writer\n' >&2
exit 2
FAKE_WRITER

    chmod 700 "$fake_bin/install" "$fake_bin/openssl" "$fake_bin/curl" "$fake_bin/docker" \
        "$fake_bin/chmod" "$fake_bin/mv" "$fake_bin/storage-env-writer"
}

setup_case() {
    local name=$1
    CASE_DIR="$TEST_ROOT/$name"
    RECORD_DIR="$CASE_DIR/records"
    RUNTIME_DIR="$CASE_DIR/runtime"
    ROOT_ENV="$CASE_DIR/root.env"
    CLOUD_ENV="$CASE_DIR/cloud.env"
    CHAT_ENV="$CASE_DIR/nexus-chat.env"
    TOKEN_FILE="$RUNTIME_DIR/secrets/cloudflared.token"
    OPENSSL_HEX_FILE="$CASE_DIR/openssl-hex.input"
    OPENSSL_BASE64_FILE="$CASE_DIR/openssl-base64.input"
    TUNNEL_TOKEN_RESPONSE_FILE="$CASE_DIR/tunnel-token-response.input"
    EXPECTED_CURL_CONFIG_FILE="$CASE_DIR/cloudflare-curl-config.expected"
    OLD_CONNECTIONS_RESPONSE_FILE="$CASE_DIR/old-connections-response.input"
    NEW_CONNECTIONS_RESPONSE_FILE="$CASE_DIR/new-connections-response.input"

    mkdir -p "$RECORD_DIR" "$RUNTIME_DIR" "$(dirname "$TOKEN_FILE")"
    cat > "$ROOT_ENV" <<'ROOT_ENV'
POSTGRES_PASSWORD=unrelated
MINIO_ROOT_USER=OLD_MINIO_USER_SENTINEL
MINIO_ROOT_PASSWORD=OLD_MINIO_PASSWORD_SENTINEL
ROOT_ENV
    cat > "$CLOUD_ENV" <<'CLOUD_ENV'
CF_API_TOKEN=CF_API_TOKEN_SENTINEL
CF_ZONE_ID=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
NEXUS_TUNNEL_ID=11111111-2222-3333-4444-555555555555
NEXUS_STORAGE_S3_ACCESS_KEY=OLD_MINIO_USER_SENTINEL
NEXUS_STORAGE_S3_SECRET_KEY=OLD_MINIO_PASSWORD_SENTINEL
CLOUD_ENV
    cat > "$CHAT_ENV" <<'CHAT_ENV'
NEXUS__STORAGE__ENDPOINT=http://127.0.0.1:9000
NEXUS__STORAGE__ACCESS_KEY=OLD_MINIO_USER_SENTINEL
NEXUS__STORAGE__SECRET_KEY=OLD_MINIO_PASSWORD_SENTINEL
NEXUS__STORAGE__BUCKET=nexus-chat-uploads
CHAT_UNRELATED=before-prepare
CHAT_ENV
    printf '%s\n' NEW_MINIO_USER_SENTINEL > "$OPENSSL_HEX_FILE"
    printf '%s\n' NEW_TUNNEL_SECRET_SENTINEL > "$OPENSSL_BASE64_FILE"
    printf '{"success":true,"result":"NEW_TUNNEL_TOKEN_SENTINEL"}\n' > "$TUNNEL_TOKEN_RESPONSE_FILE"
    printf '%s\n' OLD_TUNNEL_TOKEN_SENTINEL > "$TOKEN_FILE"
    printf 'header = "Authorization: Bearer CF_API_TOKEN_SENTINEL"\n' > "$EXPECTED_CURL_CONFIG_FILE"
    printf '{"success":true,"result":[{"id":"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee","conns":[{"client_id":"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee","is_pending_reconnect":false}]}]}\n' > "$OLD_CONNECTIONS_RESPONSE_FILE"
    printf '{"success":true,"result":[{"id":"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee","conns":[{"client_id":"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee","is_pending_reconnect":true}]},{"id":"ffffffff-1111-4222-8333-444444444444","conns":[{"client_id":"ffffffff-1111-4222-8333-444444444444","is_pending_reconnect":false}]}]}\n' > "$NEW_CONNECTIONS_RESPONSE_FILE"
    chmod 600 \
        "$ROOT_ENV" "$CLOUD_ENV" "$CHAT_ENV" "$OPENSSL_HEX_FILE" "$OPENSSL_BASE64_FILE" \
        "$TUNNEL_TOKEN_RESPONSE_FILE" "$TOKEN_FILE" "$EXPECTED_CURL_CONFIG_FILE" \
        "$OLD_CONNECTIONS_RESPONSE_FILE" "$NEW_CONNECTIONS_RESPONSE_FILE"
    write_fake_commands "$CASE_DIR"
}

run_phase() {
    local phase=$1
    local stdout_file=$2
    local stderr_file=$3
    shift 3

    (
        export PATH="$CASE_DIR/fake-bin:$PATH"
        export FAKE_RECORD_DIR="$RECORD_DIR"
        export ROTATION_ROOT_ENV="$ROOT_ENV"
        export ROTATION_CLOUD_ENV="$CLOUD_ENV"
        export ROTATION_CHAT_ENV="$CHAT_ENV"
        export FAKE_OPENSSL_HEX_FILE="$OPENSSL_HEX_FILE"
        export FAKE_OPENSSL_BASE64_FILE="$OPENSSL_BASE64_FILE"
        export FAKE_TUNNEL_TOKEN_RESPONSE_FILE="$TUNNEL_TOKEN_RESPONSE_FILE"
        export FAKE_EXPECTED_CURL_CONFIG_FILE="$EXPECTED_CURL_CONFIG_FILE"
        export FAKE_OLD_CONNECTIONS_RESPONSE_FILE="$OLD_CONNECTIONS_RESPONSE_FILE"
        export FAKE_NEW_CONNECTIONS_RESPONSE_FILE="$NEW_CONNECTIONS_RESPONSE_FILE"
        export FAKE_CF_API_BASE=https://api.cloudflare.com/client/v4
        export NEXUS_ROTATION_RUNTIME_DIR="$RUNTIME_DIR"
        export NEXUS_ROTATION_ROOT_ENV="$ROOT_ENV"
        export NEXUS_ROTATION_CLOUD_ENV="$CLOUD_ENV"
        export NEXUS_ROTATION_CHAT_ENV="$CHAT_ENV"
        export NEXUS_ROTATION_TUNNEL_TOKEN_FILE="$TOKEN_FILE"
        export NEXUS_ROTATION_TUNNEL_VERIFY_URL=https://cloud.example.test/health
        export NEXUS_ROTATION_TUNNEL_CONNECT_ATTEMPTS=1
        export NEXUS_ROTATION_TUNNEL_CONNECT_INTERVAL=0
        export NEXUS_ROTATION_MINIO_HEALTH_ATTEMPTS=1
        export NEXUS_ROTATION_MINIO_HEALTH_INTERVAL=0
        while [ "$#" -gt 0 ]; do
            export "$1"
            shift
        done
        bash "$SCRIPT" "$phase"
    ) > "$stdout_file" 2> "$stderr_file"
}

assert_captures_are_secret_free() {
    local stdout_file=$1
    local stderr_file=$2
    local secret file

    for secret in \
        CF_API_TOKEN_SENTINEL \
        EXPORTED_CF_API_TOKEN_SENTINEL \
        NEW_TUNNEL_SECRET_SENTINEL \
        NEW_TUNNEL_TOKEN_SENTINEL \
        OLD_TUNNEL_TOKEN_SENTINEL \
        NEW_MINIO_USER_SENTINEL \
        NEW_MINIO_PASSWORD_SENTINEL \
        OLD_MINIO_USER_SENTINEL \
        OLD_MINIO_PASSWORD_SENTINEL
    do
        for file in \
            "$stdout_file" \
            "$stderr_file" \
            "$RECORD_DIR/curl.argv" \
            "$RECORD_DIR/docker.argv" \
            "$RECORD_DIR/openssl.argv" \
            "$RECORD_DIR/install.argv" \
            "$RECORD_DIR/events"
        do
            [ ! -f "$file" ] || assert_file_not_contains "$file" "$secret" "secret leaked to $(basename "$file")"
        done
    done
}

assert_secret_files_are_private() {
    local secret file

    for secret in \
        CF_API_TOKEN_SENTINEL \
        EXPORTED_CF_API_TOKEN_SENTINEL \
        NEW_TUNNEL_SECRET_SENTINEL \
        NEW_TUNNEL_TOKEN_SENTINEL \
        OLD_TUNNEL_TOKEN_SENTINEL \
        NEW_MINIO_USER_SENTINEL \
        NEW_MINIO_PASSWORD_SENTINEL \
        OLD_MINIO_USER_SENTINEL \
        OLD_MINIO_PASSWORD_SENTINEL
    do
        while IFS= read -r file; do
            assert_eq 600 "$(file_mode "$file")" "secret-bearing file is not mode 0600: $file"
        done < <(grep -rlF --exclude-dir=fake-bin -- "$secret" "$CASE_DIR" 2>/dev/null || true)
    done
}

test_tunnel_adopts_dashboard_token_after_verification() {
    local out err curl_log docker_log events rotation_dir installed_hash
    setup_case tunnel-success
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    printf 'header = "Authorization: Bearer EXPORTED_CF_API_TOKEN_SENTINEL"\n' > "$EXPECTED_CURL_CONFIG_FILE"

    run_phase prepare "$out" "$err" CF_API_TOKEN=EXPORTED_CF_API_TOKEN_SENTINEL
    run_phase rotate-tunnel "$out" "$err" CF_API_TOKEN=EXPORTED_CF_API_TOKEN_SENTINEL

    curl_log="$RECORD_DIR/curl.argv"
    docker_log="$RECORD_DIR/docker.argv"
    events="$RECORD_DIR/events"
    assert_file_contains "$curl_log" '/zones/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' 'zone lookup was not used to discover the account'
    assert_file_contains "$curl_log" '/accounts/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/cfd_tunnel/11111111-2222-3333-4444-555555555555/token' 'replacement token was not fetched with GET'
    assert_file_not_contains "$curl_log" '--request PATCH' 'dashboard-governed rotation still issued tunnel PATCH'
    assert_file_contains "$curl_log" 'client_id=aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee' 'old connector cleanup was not scoped by client ID'
    assert_file_contains "$curl_log" '--request DELETE' 'old tunnel connections were not force-disconnected with DELETE'
    assert_file_not_contains "$curl_log" '--data-binary' 'dashboard-governed tunnel adoption unexpectedly sent a request body'
    assert_file_contains "$curl_log" '--connect-timeout' 'tunnel control-plane requests have no connection deadline'
    assert_file_contains "$curl_log" '--max-time' 'tunnel control-plane requests have no total deadline'
    assert_file_contains "$docker_log" 'docker rm -f cloudflared' 'existing unmanaged cloudflared container was not removed by exact name'
    assert_file_contains "$docker_log" 'cloudflared.compose.yml' 'managed cloudflared Compose file was not used'
    assert_file_contains "$docker_log" '--force-recreate cloudflared' 'cloudflared was not force-recreated'
    assert_file_order "$events" 'docker compose' 'curl GET https://cloud.example.test/health' 'public verification ran before connector replacement'
    assert_file_order "$events" 'curl GET https://cloud.example.test/health' 'curl DELETE' 'old connections were deleted before public verification'
    [ "$(count_matches "$curl_log" '/token')" -ge 2 ] || fail 'tunnel adoption did not re-fetch the dashboard token for its hash post-condition'
    assert_eq NEW_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$TOKEN_FILE")" 'replacement token file has wrong content'
    assert_eq 600 "$(file_mode "$TOKEN_FILE")" 'replacement token file is not mode 0600'
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    installed_hash="$(printf %s NEW_TUNNEL_TOKEN_SENTINEL | sha256sum | awk '{print $1}')"
    assert_eq "$installed_hash" "$(tr -d '\n' < "$rotation_dir/cloudflared.token.adopted.sha256")" 'adopted tunnel-token hash post-condition was not persisted'
    [ -f "$rotation_dir/tunnel.checkpoint" ] || fail 'tunnel checkpoint was not written after token hash verification'
    [ ! -f "$RECORD_DIR/openssl.argv" ] || fail 'tunnel adoption unexpectedly generated a tunnel secret'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_prepare_bootstraps_current_token_from_protected_container_argv() {
    local out err rotation_dir expected_hash
    setup_case tunnel-bootstrap
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    rm -f -- "$TOKEN_FILE"

    run_phase prepare "$out" "$err" \
        'FAKE_TUNNEL_CMD_JSON=["tunnel","--no-autoupdate","run","--token","OLD_TUNNEL_TOKEN_SENTINEL"]'

    assert_eq OLD_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$TOKEN_FILE")" 'prepare did not adopt the current argv token into the protected token file'
    assert_eq 600 "$(file_mode "$TOKEN_FILE")" 'bootstrapped token file is not mode 0600'
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    assert_eq OLD_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$rotation_dir/cloudflared.token.rollback")" 'prepare did not preserve the bootstrapped current token'
    expected_hash="$(printf %s OLD_TUNNEL_TOKEN_SENTINEL | sha256sum | awk '{print $1}')"
    assert_eq "$expected_hash" "$(tr -d '\n' < "$rotation_dir/cloudflared.token.rollback.sha256")" 'prepare recorded the wrong current-token hash'
    assert_file_contains "$out" 'Cloudflare dashboard' 'prepare did not give the operator an explicit dashboard-rotation instruction'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_tunnel_aborts_when_dashboard_token_is_unchanged() {
    local out err status
    setup_case tunnel-unchanged
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    printf '{"success":true,"result":"OLD_TUNNEL_TOKEN_SENTINEL"}\n' > "$TUNNEL_TOKEN_RESPONSE_FILE"

    run_phase prepare "$out" "$err"
    set +e
    run_phase rotate-tunnel "$out" "$err"
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'unchanged dashboard token did not abort rotation'
    assert_eq OLD_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$TOKEN_FILE")" 'unchanged-token abort modified the token file'
    [ ! -f "$RECORD_DIR/docker.argv" ] || fail 'unchanged-token abort replaced a connector'
    [ ! -f "$RECORD_DIR/curl.argv" ] || assert_file_not_contains "$RECORD_DIR/curl.argv" '--request DELETE' 'unchanged-token abort deleted connections'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_tunnel_failure_recovers_with_new_token_without_connection_delete() {
    local boundary failure_variable failure_value out err status docker_count rotation_dir

    for boundary in token-get install container; do
        setup_case "tunnel-recovery-$boundary"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"
        failure_variable=
        failure_value=
        case "$boundary" in
            token-get)
                failure_variable=FAKE_INSTALL_FAIL_ON
                failure_value=cloudflared.token.next
                ;;
            install)
                failure_variable=FAKE_DOCKER_FAIL_ON
                failure_value='--force-recreate cloudflared'
                ;;
            container)
                failure_variable=FAKE_CURL_FAIL_ON
                failure_value='GET https://cloud.example.test/health'
                ;;
        esac

        run_phase prepare "$out" "$err"
        set +e
        run_phase rotate-tunnel "$out" "$err" "$failure_variable=$failure_value"
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "$boundary failure unexpectedly completed tunnel rotation"
        assert_eq NEW_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$TOKEN_FILE")" "$boundary recovery did not preserve the new token"
        docker_count="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate cloudflared')"
        [ "$docker_count" -ge 1 ] || fail "$boundary recovery did not restore a connector"
        [ "$(count_matches "$RECORD_DIR/curl.argv" '/connections')" -ge 2 ] || fail "$boundary recovery did not verify a replacement connector"
        [ "$(count_matches "$RECORD_DIR/curl.argv" 'https://cloud.example.test/health')" -ge 1 ] || fail "$boundary recovery did not verify the public route"
        assert_file_not_contains "$RECORD_DIR/curl.argv" '--request DELETE' "$boundary failure deleted old connections"
        IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
        assert_eq OLD_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$rotation_dir/cloudflared.token.rollback")" "$boundary recovery overwrote the prepared old-token backup"
        assert_captures_are_secret_free "$out" "$err"
        assert_secret_files_are_private
    done
}

test_storage_rotation_commits_only_after_healthy_minio() {
    local out err root_user root_password cloud_access cloud_secret chat_access chat_secret snapshot_count
    setup_case storage-success
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=healthy

    root_user="$(env_value "$ROOT_ENV" MINIO_ROOT_USER)"
    root_password="$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)"
    cloud_access="$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)"
    cloud_secret="$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)"
    chat_access="$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)"
    chat_secret="$(env_value "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)"
    assert_eq NEW_MINIO_USER_SENTINEL "$root_user" 'root MinIO user was not replaced'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$root_password" 'root MinIO password was not replaced'
    assert_eq "$root_user" "$cloud_access" 'Cloud access key does not match MinIO root user'
    assert_eq "$root_password" "$cloud_secret" 'Cloud secret key does not match MinIO root password'
    assert_eq "$root_user" "$chat_access" 'Nexus Chat access key does not match MinIO root user'
    assert_eq "$root_password" "$chat_secret" 'Nexus Chat secret key does not match MinIO root password'
    snapshot_count="$(wc -l < "$RECORD_DIR/minio-snapshots" | tr -d ' ')"
    assert_eq 1 "$snapshot_count" 'successful storage rotation unexpectedly recreated MinIO more than once'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_storage_rotation_remains_valid_when_nexus_chat_is_not_deployed() {
    local out err rotation_dir
    setup_case storage-without-chat
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    rm -f -- "$CHAT_ENV"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy

    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    [ ! -e "$rotation_dir/chat.env.rollback" ] || \
        fail 'prepare invented a Nexus Chat rollback source for an absent deployment'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'rotation without Nexus Chat did not update MinIO'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'rotation without Nexus Chat did not update Cloud'
}

test_storage_checkpoint_can_be_explicitly_rolled_back_after_cloud_probe_failure() {
    local out err before_root before_cloud before_chat rotation_dir
    setup_case storage-explicit-rollback
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    before_root="$CASE_DIR/root.env.before"
    before_cloud="$CASE_DIR/cloud.env.before"
    before_chat="$CASE_DIR/nexus-chat.env.before"
    /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
    /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
    /usr/bin/install -m 600 "$CHAT_ENV" "$before_chat"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    printf 'CHAT_LATE_EDIT=preserved\n' >> "$CHAT_ENV"
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    [ -f "$rotation_dir/storage.checkpoint" ] || fail 'successful storage phase did not create its checkpoint'

    run_phase rollback-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy

    assert_storage_files_match "$before_root" "$before_cloud" 'explicit post-checkpoint rollback'
    assert_eq preserved "$(env_value "$CHAT_ENV" CHAT_LATE_EDIT)" \
        'explicit rollback discarded an unrelated Nexus Chat environment edit'
    assert_eq OLD_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'explicit rollback did not restore Nexus Chat access key'
    assert_eq OLD_MINIO_PASSWORD_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)" \
        'explicit rollback did not restore Nexus Chat secret key'
    assert_eq 2 "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" 'explicit rollback did not recreate MinIO with the old credential pair'
    assert_last_minio_snapshot_uses_old_pair 'explicit post-checkpoint rollback'
    [ ! -e "$rotation_dir/storage.checkpoint" ] || fail 'explicit rollback left the invalid storage checkpoint armed'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_prepare_rejects_incoherent_storage_pairs() {
    local index out err status

    for index in 0 1 2 3; do
        setup_case "prepare-incoherent-storage-$index"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"
        case "$index" in
            0)
                sed -i \
                    's/^NEXUS_STORAGE_S3_ACCESS_KEY=.*/NEXUS_STORAGE_S3_ACCESS_KEY=DIVERGENT_ACCESS_SENTINEL/' \
                    "$CLOUD_ENV"
                ;;
            1)
                sed -i \
                    's/^NEXUS_STORAGE_S3_SECRET_KEY=.*/NEXUS_STORAGE_S3_SECRET_KEY=DIVERGENT_SECRET_SENTINEL/' \
                    "$CLOUD_ENV"
                ;;
            2)
                sed -i \
                    's/^NEXUS__STORAGE__ACCESS_KEY=.*/NEXUS__STORAGE__ACCESS_KEY=DIVERGENT_ACCESS_SENTINEL/' \
                    "$CHAT_ENV"
                ;;
            3)
                sed -i \
                    's/^NEXUS__STORAGE__SECRET_KEY=.*/NEXUS__STORAGE__SECRET_KEY=DIVERGENT_SECRET_SENTINEL/' \
                    "$CHAT_ENV"
                ;;
        esac

        set +e
        run_phase prepare "$out" "$err"
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "prepare accepted incoherent storage credential case $index"
        [ ! -e "$RUNTIME_DIR/rotation.current" ] || \
            fail "prepare published incoherent rollback state for case $index"
        assert_file_contains "$err" 'one coherent access/secret pair' \
            "prepare did not explain incoherent storage credential case $index"
    done
}

test_prepare_rejects_bash_append_duplicates() {
    local out err status
    setup_case prepare-bash-append-duplicates
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    printf '%s\n' "MINIO_ROOT_USER+='_SUFFIX'" >> "$ROOT_ENV"
    printf '%s\n' "  export NEXUS_STORAGE_S3_ACCESS_KEY+='_SUFFIX'" >> "$CLOUD_ENV"
    printf '%s\n' '    NEXUS__STORAGE__ACCESS_KEY+="_SUFFIX"' >> "$CHAT_ENV"

    set +e
    run_phase prepare "$out" "$err"
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'prepare accepted coherent-looking Bash += duplicates'
    [ ! -e "$RUNTIME_DIR/rotation.current" ] || \
        fail 'prepare published active state for Bash += duplicates'
    assert_file_contains "$err" 'duplicate MINIO_ROOT_USER assignments' \
        'prepare did not classify Bash += as a duplicate assignment'
}

test_storage_rollback_refuses_to_discard_a_concurrent_credential_edit() {
    local out err rotation_dir status minio_recreates_before
    setup_case storage-concurrent-credential
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    # This simulates an authorized credential edit after the checkpoint. The
    # rollback must validate the complete affected set before changing any
    # file, rather than partially restoring earlier keys before this mismatch.
    printf 'ROOT_LATE_EDIT=preserved\n' >> "$ROOT_ENV"
    printf 'CLOUD_LATE_EDIT=preserved\n' >> "$CLOUD_ENV"
    printf 'CHAT_LATE_EDIT=preserved\n' >> "$CHAT_ENV"
    sed -i 's/^NEXUS_STORAGE_S3_SECRET_KEY=.*/NEXUS_STORAGE_S3_SECRET_KEY=CONCURRENT_SECRET_SENTINEL/' "$CLOUD_ENV"
    minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"

    set +e
    run_phase rollback-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'rollback discarded a concurrent credential edit'
    assert_eq CONCURRENT_SECRET_SENTINEL \
        "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        'rollback overwrote the concurrent credential value'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'failed rollback partially restored the root access key'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'failed rollback partially restored the root secret key'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'failed rollback partially restored the Cloud access key'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'failed rollback partially restored the Nexus Chat access key'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL \
        "$(env_value "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)" \
        'failed rollback partially restored the Nexus Chat secret key'
    assert_eq preserved "$(env_value "$ROOT_ENV" ROOT_LATE_EDIT)" \
        'failed rollback discarded an unrelated root environment edit'
    assert_eq preserved "$(env_value "$CLOUD_ENV" CLOUD_LATE_EDIT)" \
        'failed rollback discarded an unrelated Cloud environment edit'
    assert_eq preserved "$(env_value "$CHAT_ENV" CHAT_LATE_EDIT)" \
        'failed rollback discarded an unrelated Nexus Chat environment edit'
    assert_eq "$minio_recreates_before" \
        "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
        'failed validation recreated MinIO without a complete rollback set'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'failed conditional rollback removed the storage checkpoint'
    assert_file_contains "$err" 'changed outside this rotation' \
        'conditional rollback failure did not explain the concurrent edit'
}

test_storage_rollback_rejects_divergent_duplicate_credentials() {
    local out err rotation_dir status minio_recreates_before
    setup_case storage-duplicate-credential
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    printf 'NEXUS_STORAGE_S3_SECRET_KEY=CONCURRENT_DUPLICATE_SENTINEL\n' >> "$CLOUD_ENV"
    minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"

    set +e
    run_phase rollback-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'rollback accepted a divergent duplicate credential assignment'
    assert_eq CONCURRENT_DUPLICATE_SENTINEL \
        "$(env_effective_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        'rollback overwrote the effective duplicate credential'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'duplicate rejection partially restored the root access key'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'duplicate rejection partially restored the root secret key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'duplicate rejection partially restored the Cloud access key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'duplicate rejection partially restored the Chat access key'
    assert_eq "$minio_recreates_before" \
        "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
        'duplicate rejection recreated MinIO without a complete rollback set'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'duplicate rejection removed the storage checkpoint'
}

test_storage_rotation_accepts_bash_assignment_forms() {
    local out err status cloud_secret_assignment chat_secret_assignment
    setup_case storage-bash-assignment-forms
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    cloud_secret_assignment='  export NEXUS_STORAGE_S3_SECRET_KEY="OLD_MINIO_PASSWORD_SENTINEL"' # pragma: allowlist secret
    chat_secret_assignment='    NEXUS__STORAGE__SECRET_KEY="OLD_MINIO_PASSWORD_SENTINEL"' # pragma: allowlist secret

    sed -i \
        -e '1iSTORAGE_USER_BASE=OLD_MINIO' \
        -e '2iSTORAGE_PASSWORD_BASE=OLD_MINIO_PASSWORD' \
        "$ROOT_ENV"
    sed -i \
        -e "s/^MINIO_ROOT_USER=.*/  export MINIO_ROOT_USER+='OLD_MINIO_USER_SENTINEL'/" \
        -e 's/^MINIO_ROOT_PASSWORD=.*/    MINIO_ROOT_PASSWORD="${STORAGE_PASSWORD_BASE}_SENTINEL"/' \
        "$ROOT_ENV"
    sed -i \
        -e "s/^NEXUS_STORAGE_S3_ACCESS_KEY=.*/    NEXUS_STORAGE_S3_ACCESS_KEY='OLD_MINIO_USER_SENTINEL'/" \
        -e "s/^NEXUS_STORAGE_S3_SECRET_KEY=.*/$cloud_secret_assignment/" \
        "$CLOUD_ENV"
    sed -i \
        -e "s/^NEXUS__STORAGE__ACCESS_KEY=.*/  export NEXUS__STORAGE__ACCESS_KEY='OLD_MINIO_USER_SENTINEL'/" \
        -e "s/^NEXUS__STORAGE__SECRET_KEY=.*/$chat_secret_assignment/" \
        "$CHAT_ENV"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    set +e
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    status=$?
    set -e

    [ "$status" -eq 0 ] || fail 'storage rotation rejected valid indented/exported/quoted Bash assignments'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(bash_effective_env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'root access key did not rotate under Bash assignment semantics'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL \
        "$(bash_effective_env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'root secret key did not rotate under Bash assignment semantics'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(bash_effective_env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'Cloud access key did not rotate under Bash assignment semantics'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL \
        "$(bash_effective_env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        'Cloud secret key did not rotate under Bash assignment semantics'
    assert_eq NEW_MINIO_USER_SENTINEL \
        "$(bash_effective_env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'Chat access key did not rotate under Bash assignment semantics'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL \
        "$(bash_effective_env_value "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)" \
        'Chat secret key did not rotate under Bash assignment semantics'
    assert_file_not_contains "$ROOT_ENV" OLD_MINIO_USER_SENTINEL \
        'root renderer left the prior alternate assignment in place'
    assert_file_not_contains "$ROOT_ENV" OLD_MINIO_PASSWORD_SENTINEL \
        'root renderer left the prior expanded assignment in place'
    assert_file_not_contains "$CLOUD_ENV" OLD_MINIO_USER_SENTINEL \
        'Cloud renderer left the prior indented assignment in place'
    assert_file_not_contains "$CLOUD_ENV" OLD_MINIO_PASSWORD_SENTINEL \
        'Cloud renderer left the prior alternate assignment in place'
    assert_file_not_contains "$CHAT_ENV" OLD_MINIO_USER_SENTINEL \
        'Chat renderer left the prior alternate assignment in place'
    assert_file_not_contains "$CHAT_ENV" OLD_MINIO_PASSWORD_SENTINEL \
        'Chat renderer left the prior indented assignment in place'
}

test_storage_rollback_rejects_bash_form_duplicates() {
    local assignment index out err rotation_dir status minio_recreates_before
    local -a assignments=(
        'export NEXUS_STORAGE_S3_SECRET_KEY=DIVERGENT_DUPLICATE_SENTINEL'
        "    NEXUS_STORAGE_S3_SECRET_KEY='DIVERGENT_DUPLICATE_SENTINEL'" # pragma: allowlist secret
        '  export NEXUS_STORAGE_S3_SECRET_KEY="DIVERGENT_DUPLICATE_SENTINEL"' # pragma: allowlist secret
    )

    for index in "${!assignments[@]}"; do
        assignment=${assignments[$index]}
        setup_case "storage-bash-duplicate-$index"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"

        run_phase prepare "$out" "$err"
        printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
        run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
        IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
        printf '%s\n' "$assignment" >> "$CLOUD_ENV"
        minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"

        set +e
        run_phase rollback-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "rollback accepted Bash-form duplicate assignment $index"
        assert_eq DIVERGENT_DUPLICATE_SENTINEL \
            "$(bash_effective_env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
            "rollback overwrote Bash-effective duplicate assignment $index"
        assert_eq "$minio_recreates_before" \
            "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
            "Bash-form duplicate assignment $index recreated MinIO"
        [ -f "$rotation_dir/storage.checkpoint" ] || \
            fail "Bash-form duplicate assignment $index removed the storage checkpoint"
    done
}

test_storage_rollback_rejects_incomplete_staging() {
    local out err rotation_dir status minio_recreates_before
    setup_case storage-staging-failure
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"

    set +e
    run_phase rollback-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=healthy FAKE_CHMOD_FAIL_ON=rollback-next
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'rollback committed after staged-file permission setup failed'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'staging failure partially restored the root access key'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'staging failure partially restored the root secret key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'staging failure partially restored the Cloud access key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'staging failure partially restored the Chat access key'
    assert_eq "$minio_recreates_before" \
        "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
        'staging failure recreated MinIO without a complete rollback set'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'staging failure removed the storage checkpoint'
}

test_storage_rollback_respects_stable_environment_locks() {
    local out err rotation_dir status minio_recreates_before writer_pid attempt
    local writer_ready writer_release writer_out writer_err
    setup_case storage-environment-lock
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"
    writer_ready="$CASE_DIR/writer.ready"
    writer_release="$CASE_DIR/writer.release"
    writer_out="$CASE_DIR/writer.stdout"
    writer_err="$CASE_DIR/writer.stderr"
    (
        export PATH="$CASE_DIR/fake-bin:$PATH"
        export ROTATION_ROOT_ENV="$ROOT_ENV"
        export NEXUS_ROTATION_ROOT_ENV="$ROOT_ENV"
        export NEXUS_ROTATION_CLOUD_ENV="$CLOUD_ENV"
        export NEXUS_ROTATION_CHAT_ENV="$CHAT_ENV"
        export NEXUS_ROTATION_STORAGE_LOCK_TIMEOUT=1
        export FAKE_WRITER_READY_FILE="$writer_ready"
        export FAKE_WRITER_RELEASE_FILE="$writer_release"
        bash "$SCRIPT" with-storage-locks "$CASE_DIR/fake-bin/storage-env-writer"
    ) > "$writer_out" 2> "$writer_err" &
    writer_pid=$!
    for ((attempt = 1; attempt <= 100; attempt++)); do
        [ ! -e "$writer_ready" ] || break
        sleep 0.02
    done
    [ -e "$writer_ready" ] || fail 'cooperative writer did not acquire the stable environment locks'

    set +e
    run_phase rollback-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=healthy NEXUS_ROTATION_STORAGE_LOCK_TIMEOUT=0
    status=$?
    set -e
    : > "$writer_release"
    wait "$writer_pid" || fail 'cooperative writer failed after lock release'

    [ "$status" -ne 0 ] || fail 'rollback ignored a writer holding the stable environment lock'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'lock contention partially restored the root access key'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'lock contention partially restored the root secret key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'lock contention partially restored the Cloud access key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'lock contention partially restored the Chat access key'
    assert_eq "$minio_recreates_before" \
        "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
        'lock contention recreated MinIO without a complete rollback set'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'lock contention removed the storage checkpoint'
    assert_eq preserved "$(env_value "$ROOT_ENV" COOPERATIVE_LATE_EDIT)" \
        'cooperative writer edit was not preserved after rollback contention'
}

test_prepare_respects_stable_environment_locks() {
    local out err prepare_pid prepare_status writer_status attempt
    local prepare_ready prepare_release writer_ready writer_release writer_out writer_err
    setup_case prepare-environment-lock
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    prepare_ready="$CASE_DIR/prepare.ready"
    prepare_release="$CASE_DIR/prepare.release"
    writer_ready="$CASE_DIR/writer.ready"
    writer_release="$CASE_DIR/writer.release"
    writer_out="$CASE_DIR/writer.stdout"
    writer_err="$CASE_DIR/writer.stderr"
    : > "$writer_release"

    run_phase prepare "$out" "$err" \
        FAKE_INSTALL_BLOCK_ON=root.env.rollback \
        "FAKE_INSTALL_BLOCK_READY_FILE=$prepare_ready" \
        "FAKE_INSTALL_BLOCK_RELEASE_FILE=$prepare_release" &
    prepare_pid=$!
    for ((attempt = 1; attempt <= 100; attempt++)); do
        [ ! -e "$prepare_ready" ] || break
        sleep 0.02
    done
    [ -e "$prepare_ready" ] || {
        : > "$prepare_release"
        wait "$prepare_pid" || true
        fail 'prepare did not reach the environment snapshot boundary'
    }

    set +e
    (
        export PATH="$CASE_DIR/fake-bin:$PATH"
        export ROTATION_ROOT_ENV="$ROOT_ENV"
        export NEXUS_ROTATION_RUNTIME_DIR="$RUNTIME_DIR"
        export NEXUS_ROTATION_ROOT_ENV="$ROOT_ENV"
        export NEXUS_ROTATION_CLOUD_ENV="$CLOUD_ENV"
        export NEXUS_ROTATION_CHAT_ENV="$CHAT_ENV"
        export NEXUS_ROTATION_STORAGE_LOCK_TIMEOUT=0
        export FAKE_WRITER_READY_FILE="$writer_ready"
        export FAKE_WRITER_RELEASE_FILE="$writer_release"
        bash "$SCRIPT" with-storage-locks "$CASE_DIR/fake-bin/storage-env-writer"
    ) > "$writer_out" 2> "$writer_err"
    writer_status=$?
    set -e
    : > "$prepare_release"
    set +e
    wait "$prepare_pid"
    prepare_status=$?
    set -e

    [ "$prepare_status" -eq 0 ] || fail 'lock-holding prepare did not complete after release'
    [ "$writer_status" -ne 0 ] || fail 'prepare allowed a cooperative writer during its environment snapshot'
    [ ! -e "$writer_ready" ] || fail 'cooperative writer entered while prepare was snapshotting'
    if env_value "$ROOT_ENV" COOPERATIVE_LATE_EDIT >/dev/null 2>&1; then
        fail 'prepare allowed a cooperative environment edit during snapshot'
    fi
    assert_file_contains "$writer_err" 'storage credential files are being updated by another writer' \
        'prepare lock contention was not explicit'
}

test_prepare_rejects_duplicate_credentials_before_state() {
    local out err status
    setup_case storage-duplicate-before-rotation
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    printf 'NEXUS_STORAGE_S3_SECRET_KEY=DIVERGENT_DUPLICATE_SENTINEL\n' >> "$CLOUD_ENV"

    set +e
    run_phase prepare "$out" "$err"
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'prepare accepted a duplicate protected credential'
    assert_eq OLD_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'duplicate prepare failure changed the root access key'
    assert_eq OLD_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'duplicate prepare failure changed the root secret key'
    assert_eq OLD_MINIO_USER_SENTINEL "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'duplicate prepare failure changed the Cloud access key'
    assert_eq DIVERGENT_DUPLICATE_SENTINEL \
        "$(env_effective_value "$CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        'duplicate prepare failure discarded the effective duplicate value'
    [ ! -f "$RECORD_DIR/minio-snapshots" ] || \
        fail 'duplicate prepare failure recreated MinIO'
    [ ! -e "$RUNTIME_DIR/rotation.current" ] || \
        fail 'duplicate prepare failure published active rollback state'
    assert_file_contains "$err" 'duplicate NEXUS_STORAGE_S3_SECRET_KEY assignments' \
        'duplicate prepare failure did not identify the ambiguity'
}

test_storage_rollback_compensates_a_commit_failure() {
    local out err rotation_dir status minio_recreates_before
    setup_case storage-commit-compensation
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    minio_recreates_before="$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')"

    set +e
    run_phase rollback-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=healthy FAKE_MV_FAIL_ALWAYS_MATCH=.cloud.env.rollback-next.
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'rollback ignored a staged Cloud commit failure'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_USER)" \
        'Cloud commit failure left root access partially restored'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$(env_value "$ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        'Cloud commit failure left root secret partially restored'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        'Cloud commit failure changed the Cloud access key'
    assert_eq NEW_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'Cloud commit failure changed the Chat access key'
    assert_eq "$minio_recreates_before" \
        "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" \
        'commit failure recreated MinIO without a complete rollback set'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'commit failure removed the storage checkpoint'
}

test_storage_rollback_preserves_artifacts_when_compensation_fails() {
    local out err rotation_dir status snapshot_count
    setup_case storage-compensation-failure
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    run_phase rotate-storage "$out" "$err" FAKE_MINIO_HEALTH=healthy
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"

    set +e
    run_phase rollback-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=healthy \
        FAKE_MV_FAIL_ALWAYS_MATCH=.cloud.env.rollback-next. \
        FAKE_MV_FAIL_ALWAYS_MATCH_2=.root.env.rollback-source.
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'rollback reported success after compensation failed'
    snapshot_count="$(find "$CASE_DIR" -maxdepth 1 -type f -name '.root.env.rollback-source.*' | wc -l | tr -d ' ')"
    [ "$snapshot_count" -ge 1 ] || fail 'compensation failure discarded the protected root recovery snapshot'
    assert_file_contains "$err" 'compensation failed' \
        'compensation failure was not escalated for immediate operator attention'
    [ -f "$rotation_dir/storage.checkpoint" ] || \
        fail 'compensation failure removed the storage checkpoint'
}

test_storage_failure_restores_both_envs_and_recreates_old_minio() {
    local out err before_root before_cloud before_chat status snapshot_count first_snapshot second_snapshot first_credentials second_credentials
    setup_case storage-rollback
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    before_root="$CASE_DIR/root.env.before"
    before_cloud="$CASE_DIR/cloud.env.before"
    before_chat="$CASE_DIR/nexus-chat.env.before"
    /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
    /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
    /usr/bin/install -m 600 "$CHAT_ENV" "$before_chat"

    run_phase prepare "$out" "$err"
    printf 'ROOT_UNRELATED=after-prepare\n' >> "$ROOT_ENV"
    printf 'CLOUD_UNRELATED=after-prepare\n' >> "$CLOUD_ENV"
    printf 'CHAT_LATE_EDIT=preserved\n' >> "$CHAT_ENV"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"
    set +e
    run_phase rotate-storage "$out" "$err" \
        FAKE_MINIO_HEALTH=unhealthy
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'unhealthy MinIO did not fail the storage phase'
    assert_eq after-prepare "$(env_value "$ROOT_ENV" ROOT_UNRELATED)" \
        'pre-health rollback discarded an unrelated root environment edit'
    assert_eq after-prepare "$(env_value "$CLOUD_ENV" CLOUD_UNRELATED)" \
        'pre-health rollback discarded an unrelated Cloud environment edit'
    assert_eq preserved "$(env_value "$CHAT_ENV" CHAT_LATE_EDIT)" \
        'pre-health rollback discarded an unrelated Nexus Chat environment edit'
    assert_eq OLD_MINIO_USER_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__ACCESS_KEY)" \
        'pre-health rollback did not restore Nexus Chat access key'
    assert_eq OLD_MINIO_PASSWORD_SENTINEL "$(env_value "$CHAT_ENV" NEXUS__STORAGE__SECRET_KEY)" \
        'pre-health rollback did not restore Nexus Chat secret key'
    snapshot_count="$(wc -l < "$RECORD_DIR/minio-snapshots" | tr -d ' ')"
    assert_eq 2 "$snapshot_count" 'rollback did not recreate MinIO after restoring old credentials'
    first_snapshot="$(sed -n '1p' "$RECORD_DIR/minio-snapshots")"
    second_snapshot="$(sed -n '2p' "$RECORD_DIR/minio-snapshots")"
    first_credentials=${first_snapshot#minio-up 1 }
    second_credentials=${second_snapshot#minio-up 2 }
    [ "$first_credentials" != "$second_credentials" ] || fail 'rollback MinIO recreation did not switch back to old credentials'
    assert_text_contains "$second_snapshot" "root_user=$(printf '%s' OLD_MINIO_USER_SENTINEL | sha256sum | awk '{print $1}')" 'rollback MinIO user hash is not the old value'
    assert_text_contains "$second_snapshot" "root_password=$(printf '%s' OLD_MINIO_PASSWORD_SENTINEL | sha256sum | awk '{print $1}')" 'rollback MinIO password hash is not the old value'
    assert_text_contains "$second_snapshot" "cloud_access=$(printf '%s' OLD_MINIO_USER_SENTINEL | sha256sum | awk '{print $1}')" 'rollback Cloud access key hash is not the old value'
    assert_text_contains "$second_snapshot" "cloud_secret=$(printf '%s' OLD_MINIO_PASSWORD_SENTINEL | sha256sum | awk '{print $1}')" 'rollback Cloud secret key hash is not the old value'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

assert_storage_files_match() {
    local before_root=$1
    local before_cloud=$2
    local message=$3

    cmp -s "$before_root" "$ROOT_ENV" || fail "$message: root environment was not restored byte-for-byte"
    cmp -s "$before_cloud" "$CLOUD_ENV" || fail "$message: Cloud environment was not restored byte-for-byte"
}

assert_last_minio_snapshot_uses_old_pair() {
    local message=$1
    local snapshot

    snapshot="$(tail -1 "$RECORD_DIR/minio-snapshots")"
    assert_text_contains "$snapshot" "root_user=$(printf '%s' OLD_MINIO_USER_SENTINEL | sha256sum | awk '{print $1}')" "$message: MinIO user is not the old value"
    assert_text_contains "$snapshot" "root_password=$(printf '%s' OLD_MINIO_PASSWORD_SENTINEL | sha256sum | awk '{print $1}')" "$message: MinIO password is not the old value"
    assert_text_contains "$snapshot" "cloud_access=$(printf '%s' OLD_MINIO_USER_SENTINEL | sha256sum | awk '{print $1}')" "$message: Cloud access key is not the old value"
    assert_text_contains "$snapshot" "cloud_secret=$(printf '%s' OLD_MINIO_PASSWORD_SENTINEL | sha256sum | awk '{print $1}')" "$message: Cloud secret key is not the old value"
    assert_text_contains "$snapshot" "chat_access=$(printf '%s' OLD_MINIO_USER_SENTINEL | sha256sum | awk '{print $1}')" "$message: Nexus Chat access key is not the old value"
    assert_text_contains "$snapshot" "chat_secret=$(printf '%s' OLD_MINIO_PASSWORD_SENTINEL | sha256sum | awk '{print $1}')" "$message: Nexus Chat secret key is not the old value"
}

test_each_storage_replacement_failure_restores_the_pair() {
    local replacement out err before_root before_cloud before_chat status

    for replacement in 1 2 3 4 5 6; do
        setup_case "storage-replacement-failure-$replacement"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"
        before_root="$CASE_DIR/root.env.before"
        before_cloud="$CASE_DIR/cloud.env.before"
        before_chat="$CASE_DIR/nexus-chat.env.before"
        /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
        /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
        /usr/bin/install -m 600 "$CHAT_ENV" "$before_chat"
        run_phase prepare "$out" "$err"
        printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"

        set +e
        run_phase rotate-storage "$out" "$err" \
            "FAKE_INSTALL_FAIL_ENV_REPLACE_NUMBER=$replacement"
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "storage replacement $replacement failure unexpectedly succeeded"
        assert_storage_files_match "$before_root" "$before_cloud" "storage replacement $replacement failure"
        cmp -s "$before_chat" "$CHAT_ENV" || fail "storage replacement $replacement failure: Nexus Chat environment was not restored"
        assert_last_minio_snapshot_uses_old_pair "storage replacement $replacement rollback"
        assert_file_contains "$RECORD_DIR/docker.argv" '--force-recreate minio' "storage replacement $replacement rollback did not recreate MinIO"
        assert_captures_are_secret_free "$out" "$err"
        assert_secret_files_are_private
    done
}

test_storage_docker_failure_restores_the_pair() {
    local out err before_root before_cloud status
    setup_case storage-docker-failure
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    before_root="$CASE_DIR/root.env.before"
    before_cloud="$CASE_DIR/cloud.env.before"
    /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
    /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"

    set +e
    run_phase rotate-storage "$out" "$err" FAKE_DOCKER_FAIL_ON=--force-recreate\ minio
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'MinIO recreation failure unexpectedly succeeded'
    assert_storage_files_match "$before_root" "$before_cloud" 'MinIO recreation failure'
    assert_eq 2 "$(count_matches "$RECORD_DIR/docker.argv" '--force-recreate minio')" 'MinIO recreation failure did not exercise rollback recreation'
    assert_last_minio_snapshot_uses_old_pair 'MinIO recreation failure rollback'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_storage_signals_restore_the_pair() {
    local signal replacement out err before_root before_cloud status

    for signal in INT TERM; do
        case "$signal" in
            INT) replacement=2 ;;
            TERM) replacement=4 ;;
        esac
        setup_case "storage-signal-${signal,,}"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"
        before_root="$CASE_DIR/root.env.before"
        before_cloud="$CASE_DIR/cloud.env.before"
        /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
        /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
        run_phase prepare "$out" "$err"
        printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"

        set +e
        run_phase rotate-storage "$out" "$err" \
            "FAKE_INSTALL_SIGNAL_ENV_REPLACE_NUMBER=$replacement" \
            "FAKE_INSTALL_SIGNAL=$signal"
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "$signal during storage replacement unexpectedly succeeded"
        assert_storage_files_match "$before_root" "$before_cloud" "$signal during storage replacement"
        assert_last_minio_snapshot_uses_old_pair "$signal storage rollback"
        assert_captures_are_secret_free "$out" "$err"
        assert_secret_files_are_private
    done
}

test_storage_rollback_waits_for_restored_minio_health() {
    local out err before_root before_cloud status sequence
    setup_case storage-rollback-health
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    before_root="$CASE_DIR/root.env.before"
    before_cloud="$CASE_DIR/cloud.env.before"
    sequence="$CASE_DIR/minio-health.sequence"
    /usr/bin/install -m 600 "$ROOT_ENV" "$before_root"
    /usr/bin/install -m 600 "$CLOUD_ENV" "$before_cloud"
    printf 'unhealthy\nhealthy\n' > "$sequence"
    chmod 600 "$sequence"
    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"

    set +e
    run_phase rotate-storage "$out" "$err" \
        "FAKE_MINIO_HEALTH_SEQUENCE_FILE=$sequence"
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'failed new MinIO health unexpectedly succeeded'
    assert_storage_files_match "$before_root" "$before_cloud" 'restored MinIO health wait'
    assert_eq 2 "$(tr -d '\n' < "$RECORD_DIR/minio-inspect.count")" 'rollback did not wait for restored MinIO health'
    assert_file_contains "$err" 'paired storage rollback completed' 'rollback claimed no successful restored-health checkpoint'
    assert_last_minio_snapshot_uses_old_pair 'restored-health rollback'
}

test_storage_rollback_health_failure_requires_attention() {
    local out err status sequence
    setup_case storage-rollback-health-failure
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"
    sequence="$CASE_DIR/minio-health.sequence"
    printf 'unhealthy\nunhealthy\n' > "$sequence"
    chmod 600 "$sequence"
    run_phase prepare "$out" "$err"
    printf '%s\n' NEW_MINIO_PASSWORD_SENTINEL > "$OPENSSL_BASE64_FILE"

    set +e
    run_phase rotate-storage "$out" "$err" \
        "FAKE_MINIO_HEALTH_SEQUENCE_FILE=$sequence"
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'failed rollback health unexpectedly succeeded'
    assert_file_contains "$err" 'requires immediate operator attention' 'failed restored-health checkpoint did not demand operator attention'
    assert_file_not_contains "$err" 'paired storage rollback completed' 'failed restored-health checkpoint was claimed complete'
}

test_single_operator_lock_serializes_fixed_rotation_state() {
    local first_out first_err second_out second_err ready release first_pid first_status second_status attempt
    setup_case operator-lock
    first_out="$CASE_DIR/first.stdout"
    first_err="$CASE_DIR/first.stderr"
    second_out="$CASE_DIR/second.stdout"
    second_err="$CASE_DIR/second.stderr"
    ready="$CASE_DIR/install-block.ready"
    release="$CASE_DIR/install-block.release"

    run_phase prepare "$first_out" "$first_err" \
        FAKE_INSTALL_BLOCK_ON=root.env.rollback \
        "FAKE_INSTALL_BLOCK_READY_FILE=$ready" \
        "FAKE_INSTALL_BLOCK_RELEASE_FILE=$release" &
    first_pid=$!
    for ((attempt = 1; attempt <= 100; attempt++)); do
        [ ! -e "$ready" ] || break
        sleep 0.05
    done
    [ -e "$ready" ] || {
        wait "$first_pid" || true
        fail 'first operator did not reach the lock-held boundary'
    }

    set +e
    run_phase prepare "$second_out" "$second_err"
    second_status=$?
    set -e
    : > "$release"
    set +e
    wait "$first_pid"
    first_status=$?
    set -e

    [ "$first_status" -eq 0 ] || fail 'lock-holding operator did not complete prepare'
    [ "$second_status" -ne 0 ] || fail 'second operator entered fixed rotation state concurrently'
    assert_file_contains "$second_err" 'another rotation operator is active' 'lock rejection was not explicit'
}

test_cleanup_removes_only_active_rotation_state() {
    local out err rotation_dir status
    setup_case cleanup
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    [ -d "$rotation_dir" ] || fail 'prepare did not create active rollback state'
    /usr/bin/install -m 600 /dev/null "$rotation_dir/tunnel.checkpoint"
    /usr/bin/install -m 600 /dev/null "$rotation_dir/storage.checkpoint"
    set +e
    run_phase cleanup "$out" "$err"
    status=$?
    set -e

    [ "$status" -eq 0 ] || fail 'cleanup phase is not implemented'
    [ ! -e "$rotation_dir" ] || fail 'cleanup left the active rollback directory behind'
    [ ! -e "$RUNTIME_DIR/rotation.current" ] || fail 'cleanup left the active state pointer behind'
    [ -d "$RUNTIME_DIR" ] || fail 'cleanup removed the containing runtime directory'
    [ -f "$ROOT_ENV" ] || fail 'cleanup removed the root environment fixture'
    [ -f "$CLOUD_ENV" ] || fail 'cleanup removed the Cloud environment fixture'
}

test_cleanup_requires_both_completed_rotation_checkpoints() {
    local missing out err rotation_dir status

    for missing in tunnel storage; do
        setup_case "cleanup-missing-$missing"
        out="$CASE_DIR/stdout"
        err="$CASE_DIR/stderr"
        run_phase prepare "$out" "$err"
        IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
        case "$missing" in
            tunnel) /usr/bin/install -m 600 /dev/null "$rotation_dir/storage.checkpoint" ;;
            storage) /usr/bin/install -m 600 /dev/null "$rotation_dir/tunnel.checkpoint" ;;
        esac

        set +e
        run_phase cleanup "$out" "$err"
        status=$?
        set -e

        [ "$status" -ne 0 ] || fail "cleanup accepted a missing $missing checkpoint"
        [ -d "$rotation_dir" ] || fail "cleanup removed rollback state without the $missing checkpoint"
        [ -f "$RUNTIME_DIR/rotation.current" ] || fail "cleanup removed active state without the $missing checkpoint"
    done
}

run_test() {
    local name=$1
    shift
    "$@"
    passes=$((passes + 1))
    printf 'ok %d - %s\n' "$passes" "$name"
}

run_test 'tunnel adopts a changed dashboard token and deletes old connections only after verification' \
    test_tunnel_adopts_dashboard_token_after_verification
run_test 'prepare bootstraps the current argv token without exposing it' \
    test_prepare_bootstraps_current_token_from_protected_container_argv
run_test 'tunnel aborts before connector replacement when the dashboard token is unchanged' \
    test_tunnel_aborts_when_dashboard_token_is_unchanged
run_test 'tunnel failures recover with the new token and leave old connections untouched' \
    test_tunnel_failure_recovers_with_new_token_without_connection_delete
run_test 'storage rotation commits matching credentials only after MinIO is healthy' \
    test_storage_rotation_commits_only_after_healthy_minio
run_test 'storage rotation conditionally skips Nexus Chat when it is not deployed' \
    test_storage_rotation_remains_valid_when_nexus_chat_is_not_deployed
run_test 'prepare rejects incoherent cross-file storage credential pairs' \
    test_prepare_rejects_incoherent_storage_pairs
run_test 'prepare rejects coherent-looking Bash append duplicates' \
    test_prepare_rejects_bash_append_duplicates
run_test 'storage checkpoint remains explicitly rollback-capable after a Cloud probe failure' \
    test_storage_checkpoint_can_be_explicitly_rolled_back_after_cloud_probe_failure
run_test 'storage rollback preserves a concurrent credential edit and fails closed' \
    test_storage_rollback_refuses_to_discard_a_concurrent_credential_edit
run_test 'storage rollback rejects divergent duplicate credentials without partial mutation' \
    test_storage_rollback_rejects_divergent_duplicate_credentials
run_test 'storage rotation accepts Bash-effective indented, exported, quoted, expanded, and append assignments' \
    test_storage_rotation_accepts_bash_assignment_forms
run_test 'storage rollback rejects Bash-form duplicate credential assignments' \
    test_storage_rollback_rejects_bash_form_duplicates
run_test 'storage rollback rejects incomplete staging without partial mutation' \
    test_storage_rollback_rejects_incomplete_staging
run_test 'storage rollback respects stable per-environment writer locks' \
    test_storage_rollback_respects_stable_environment_locks
run_test 'prepare respects stable per-environment writer locks while snapshotting' \
    test_prepare_respects_stable_environment_locks
run_test 'prepare rejects duplicate protected credentials before publishing state' \
    test_prepare_rejects_duplicate_credentials_before_state
run_test 'storage rollback compensates a staged commit failure' \
    test_storage_rollback_compensates_a_commit_failure
run_test 'storage rollback preserves protected artifacts when compensation fails' \
    test_storage_rollback_preserves_artifacts_when_compensation_fails
run_test 'pre-health storage failure restores the atomic credential set and recreates old MinIO' \
    test_storage_failure_restores_both_envs_and_recreates_old_minio
run_test 'each of the six atomic storage replacements rolls back on failure' \
    test_each_storage_replacement_failure_restores_the_pair
run_test 'Docker failure after all storage replacements restores the old pair' \
    test_storage_docker_failure_restores_the_pair
run_test 'INT and TERM during atomic storage replacements restore all credential keys' \
    test_storage_signals_restore_the_pair
run_test 'storage rollback waits for restored MinIO health before completion' \
    test_storage_rollback_waits_for_restored_minio_health
run_test 'failed restored MinIO health is never claimed as completed rollback' \
    test_storage_rollback_health_failure_requires_attention
run_test 'single-operator lock serializes fixed rotation state' \
    test_single_operator_lock_serializes_fixed_rotation_state
run_test 'cleanup removes only the active protected rollback state' \
    test_cleanup_removes_only_active_rotation_state
run_test 'cleanup requires both tunnel and storage checkpoints' \
    test_cleanup_requires_both_completed_rotation_checkpoints

printf 'PASS: %d rotation tests\n' "$passes"
