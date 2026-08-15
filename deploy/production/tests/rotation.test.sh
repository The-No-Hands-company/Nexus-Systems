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
{
    printf 'install'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/install.argv"
exec /usr/bin/install "$@"
FAKE_INSTALL

    cat > "$fake_bin/openssl" <<'FAKE_OPENSSL'
#!/usr/bin/env bash
set -euo pipefail
umask 077
{
    printf 'openssl'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/openssl.argv"

case " $* " in
    *' -hex '*) /bin/cat "$FAKE_OPENSSL_HEX_FILE" ;;
    *' -base64 '*) /bin/cat "$FAKE_OPENSSL_BASE64_FILE" ;;
    *) printf 'unsupported fake openssl invocation\n' >&2; exit 2 ;;
esac
FAKE_OPENSSL

    cat > "$fake_bin/curl" <<'FAKE_CURL'
#!/usr/bin/env bash
set -euo pipefail
umask 077
{
    printf 'curl'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/curl.argv"

method=GET
output=
url=
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
        --config|-K|--header|-H|--data-binary)
            shift 2
            ;;
        --silent|--show-error|--fail-with-body)
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

if [ -n "${FAKE_CURL_FAIL_ON:-}" ] && [[ "$method $url" == *"$FAKE_CURL_FAIL_ON"* ]]; then
    printf 'simulated curl failure\n' >&2
    exit 22
fi

case "$method $url" in
    "GET "*'/zones/'*)
        printf '{"success":true,"result":{"account":{"id":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}}}\n' > "$output"
        ;;
    "PATCH "*'/cfd_tunnel/'*)
        printf '{"success":true,"result":{"id":"11111111-2222-3333-4444-555555555555"}}\n' > "$output"
        ;;
    "GET "*'/token')
        /bin/cat "$FAKE_TUNNEL_TOKEN_RESPONSE_FILE" > "$output"
        ;;
    "DELETE "*'/connections')
        printf '{"success":true,"result":{}}\n' > "$output"
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
{
    printf 'docker'
    printf ' %q' "$@"
    printf '\n'
} >> "$FAKE_RECORD_DIR/docker.argv"

env_hash() {
    local file=$1
    local key=$2
    local line value

    value=
    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            "$key="*) value=${line#*=}; break ;;
        esac
    done < "$file"
    printf '%s' "$value" | sha256sum | awk '{print $1}'
}

if [ "${1:-}" = inspect ]; then
    printf '%s\n' "${FAKE_MINIO_HEALTH:-healthy}"
    exit 0
fi

if [[ " $* " == *' up '* ]] && [[ " $* " == *' minio '* ]]; then
    counter_file="$FAKE_RECORD_DIR/minio-up.count"
    count=0
    [ ! -f "$counter_file" ] || read -r count < "$counter_file"
    count=$((count + 1))
    printf '%s\n' "$count" > "$counter_file"
    printf 'minio-up %s root_user=%s root_password=%s cloud_access=%s cloud_secret=%s\n' \
        "$count" \
        "$(env_hash "$ROTATION_ROOT_ENV" MINIO_ROOT_USER)" \
        "$(env_hash "$ROTATION_ROOT_ENV" MINIO_ROOT_PASSWORD)" \
        "$(env_hash "$ROTATION_CLOUD_ENV" NEXUS_STORAGE_S3_ACCESS_KEY)" \
        "$(env_hash "$ROTATION_CLOUD_ENV" NEXUS_STORAGE_S3_SECRET_KEY)" \
        >> "$FAKE_RECORD_DIR/minio-snapshots"
fi

if [ -n "${FAKE_DOCKER_FAIL_ON:-}" ] && [[ " $* " == *"$FAKE_DOCKER_FAIL_ON"* ]]; then
    printf 'simulated docker failure\n' >&2
    exit 1
fi
FAKE_DOCKER

    chmod 700 "$fake_bin/install" "$fake_bin/openssl" "$fake_bin/curl" "$fake_bin/docker"
}

setup_case() {
    local name=$1
    CASE_DIR="$TEST_ROOT/$name"
    RECORD_DIR="$CASE_DIR/records"
    RUNTIME_DIR="$CASE_DIR/runtime"
    ROOT_ENV="$CASE_DIR/root.env"
    CLOUD_ENV="$CASE_DIR/cloud.env"
    TOKEN_FILE="$RUNTIME_DIR/secrets/cloudflared.token"
    OPENSSL_HEX_FILE="$CASE_DIR/openssl-hex.input"
    OPENSSL_BASE64_FILE="$CASE_DIR/openssl-base64.input"
    TUNNEL_TOKEN_RESPONSE_FILE="$CASE_DIR/tunnel-token-response.input"

    mkdir -p "$RECORD_DIR" "$RUNTIME_DIR"
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
    printf '%s\n' NEW_MINIO_USER_SENTINEL > "$OPENSSL_HEX_FILE"
    printf '%s\n' NEW_TUNNEL_SECRET_SENTINEL > "$OPENSSL_BASE64_FILE"
    printf '{"success":true,"result":"NEW_TUNNEL_TOKEN_SENTINEL"}\n' > "$TUNNEL_TOKEN_RESPONSE_FILE"
    chmod 600 "$ROOT_ENV" "$CLOUD_ENV" "$OPENSSL_HEX_FILE" "$OPENSSL_BASE64_FILE" "$TUNNEL_TOKEN_RESPONSE_FILE"
    write_fake_commands "$CASE_DIR"
}

run_phase() {
    local phase=$1
    local stdout_file=$2
    local stderr_file=$3
    shift 3

    env \
        PATH="$CASE_DIR/fake-bin:$PATH" \
        FAKE_RECORD_DIR="$RECORD_DIR" \
        ROTATION_ROOT_ENV="$ROOT_ENV" \
        ROTATION_CLOUD_ENV="$CLOUD_ENV" \
        FAKE_OPENSSL_HEX_FILE="$OPENSSL_HEX_FILE" \
        FAKE_OPENSSL_BASE64_FILE="$OPENSSL_BASE64_FILE" \
        FAKE_TUNNEL_TOKEN_RESPONSE_FILE="$TUNNEL_TOKEN_RESPONSE_FILE" \
        NEXUS_ROTATION_RUNTIME_DIR="$RUNTIME_DIR" \
        NEXUS_ROTATION_ROOT_ENV="$ROOT_ENV" \
        NEXUS_ROTATION_CLOUD_ENV="$CLOUD_ENV" \
        NEXUS_ROTATION_TUNNEL_TOKEN_FILE="$TOKEN_FILE" \
        NEXUS_ROTATION_MINIO_HEALTH_ATTEMPTS=1 \
        NEXUS_ROTATION_MINIO_HEALTH_INTERVAL=0 \
        "$@" \
        bash "$SCRIPT" "$phase" > "$stdout_file" 2> "$stderr_file"
}

assert_captures_are_secret_free() {
    local stdout_file=$1
    local stderr_file=$2
    local secret file

    for secret in \
        CF_API_TOKEN_SENTINEL \
        NEW_TUNNEL_SECRET_SENTINEL \
        NEW_TUNNEL_TOKEN_SENTINEL \
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
            "$RECORD_DIR/install.argv"
        do
            [ ! -f "$file" ] || assert_file_not_contains "$file" "$secret" "secret leaked to $(basename "$file")"
        done
    done
}

assert_secret_files_are_private() {
    local secret file

    for secret in \
        CF_API_TOKEN_SENTINEL \
        NEW_TUNNEL_SECRET_SENTINEL \
        NEW_TUNNEL_TOKEN_SENTINEL \
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

test_tunnel_rotation_uses_files_and_official_api_sequence() {
    local out err curl_log docker_log
    setup_case tunnel-success
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    run_phase rotate-tunnel "$out" "$err"

    curl_log="$RECORD_DIR/curl.argv"
    docker_log="$RECORD_DIR/docker.argv"
    assert_file_contains "$curl_log" '/zones/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' 'zone lookup was not used to discover the account'
    assert_file_contains "$curl_log" '--request PATCH' 'tunnel secret was not rotated with PATCH'
    assert_file_contains "$curl_log" '/accounts/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/cfd_tunnel/11111111-2222-3333-4444-555555555555/token' 'replacement token was not fetched with GET'
    assert_file_contains "$curl_log" '--request DELETE' 'old tunnel connections were not force-disconnected with DELETE'
    assert_file_contains "$curl_log" '/connections' 'connection cleanup endpoint was not called'
    assert_file_contains "$docker_log" 'docker rm -f cloudflared' 'existing unmanaged cloudflared container was not removed by exact name'
    assert_file_contains "$docker_log" 'cloudflared.compose.yml' 'managed cloudflared Compose file was not used'
    assert_file_contains "$docker_log" '--force-recreate cloudflared' 'cloudflared was not force-recreated'
    assert_eq NEW_TUNNEL_TOKEN_SENTINEL "$(tr -d '\n' < "$TOKEN_FILE")" 'replacement token file has wrong content'
    assert_eq 600 "$(file_mode "$TOKEN_FILE")" 'replacement token file is not mode 0600'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_storage_rotation_commits_only_after_healthy_minio() {
    local out err root_user root_password cloud_access cloud_secret snapshot_count
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
    assert_eq NEW_MINIO_USER_SENTINEL "$root_user" 'root MinIO user was not replaced'
    assert_eq NEW_MINIO_PASSWORD_SENTINEL "$root_password" 'root MinIO password was not replaced'
    assert_eq "$root_user" "$cloud_access" 'Cloud access key does not match MinIO root user'
    assert_eq "$root_password" "$cloud_secret" 'Cloud secret key does not match MinIO root password'
    snapshot_count="$(wc -l < "$RECORD_DIR/minio-snapshots" | tr -d ' ')"
    assert_eq 1 "$snapshot_count" 'successful storage rotation unexpectedly recreated MinIO more than once'
    assert_captures_are_secret_free "$out" "$err"
    assert_secret_files_are_private
}

test_storage_failure_restores_both_envs_and_recreates_old_minio() {
    local out err before_root before_cloud status snapshot_count first_snapshot second_snapshot first_credentials second_credentials
    setup_case storage-rollback
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
        FAKE_MINIO_HEALTH=unhealthy
    status=$?
    set -e

    [ "$status" -ne 0 ] || fail 'unhealthy MinIO did not fail the storage phase'
    cmp -s "$before_root" "$ROOT_ENV" || fail 'root environment was not restored byte-for-byte'
    cmp -s "$before_cloud" "$CLOUD_ENV" || fail 'Cloud environment was not restored byte-for-byte'
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

test_cleanup_removes_only_active_rotation_state() {
    local out err rotation_dir status
    setup_case cleanup
    out="$CASE_DIR/stdout"
    err="$CASE_DIR/stderr"

    run_phase prepare "$out" "$err"
    IFS= read -r rotation_dir < "$RUNTIME_DIR/rotation.current"
    [ -d "$rotation_dir" ] || fail 'prepare did not create active rollback state'
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

run_test() {
    local name=$1
    shift
    "$@"
    passes=$((passes + 1))
    printf 'ok %d - %s\n' "$passes" "$name"
}

run_test 'tunnel rotation uses protected files and Cloudflare PATCH/GET/DELETE semantics' \
    test_tunnel_rotation_uses_files_and_official_api_sequence
run_test 'storage rotation commits matching credentials only after MinIO is healthy' \
    test_storage_rotation_commits_only_after_healthy_minio
run_test 'pre-health storage failure restores both environments and recreates old MinIO' \
    test_storage_failure_restores_both_envs_and_recreates_old_minio
run_test 'cleanup removes only the active protected rollback state' \
    test_cleanup_removes_only_active_rotation_state

printf 'PASS: %d rotation tests\n' "$passes"
