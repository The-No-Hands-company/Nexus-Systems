#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCTION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-processes-test.XXXXXX")"
FIXTURE_DIR="$TEST_ROOT/fixtures"
SERVICE_DIR="$TEST_ROOT/service"
PID_DIR="$TEST_ROOT/pids"
PROC_ROOT="$TEST_ROOT/proc"
mkdir -p "$FIXTURE_DIR" "$SERVICE_DIR" "$PID_DIR" "$PROC_ROOT"

cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT

cat > "$FIXTURE_DIR/ss" <<'FIXTURE'
#!/usr/bin/env bash
if [ "${SS_FAIL:-0}" = "1" ]; then
    exit 1
fi
if [ "${SS_REQUIRE_PID_FILE:-0}" = 1 ] && [ ! -f "${SS_PID_FILE:-}" ]; then
    exit 0
fi
output=${SS_OUTPUT:-}
if [ -n "${SS_PID_FILE:-}" ] && [ -f "$SS_PID_FILE" ]; then
    pid=
    IFS= read -r pid < "$SS_PID_FILE" || true
    output=${output//__PID__/$pid}
fi
printf '%s\n' "$output"
FIXTURE

cat > "$FIXTURE_DIR/readlink" <<'FIXTURE'
#!/usr/bin/env bash
printf '%s\n' "${READLINK_OUTPUT:-}"
FIXTURE

cat > "$FIXTURE_DIR/curl" <<'FIXTURE'
#!/usr/bin/env bash
printf '%q ' "$@" >> "$CURL_RECORD"
printf '\n' >> "$CURL_RECORD"
[ "${FAKE_CURL_FAIL:-0}" != 1 ] || exit 22
if [ -n "${FAKE_CURL_BODY+x}" ]; then
    printf '%s\n' "$FAKE_CURL_BODY"
else
    printf '%s\n' '{"ok":true}'
fi
FIXTURE

cat > "$FIXTURE_DIR/kill" <<'FIXTURE'
#!/usr/bin/env bash
printf '%q ' "$@" >> "$KILL_RECORD"
printf '\n' >> "$KILL_RECORD"
exit "${FAKE_KILL_STATUS:-0}"
FIXTURE

cat > "$FIXTURE_DIR/caddy" <<'FIXTURE'
#!/usr/bin/env bash
printf '%q ' "$@" >> "$CADDY_RECORD"
printf '\n' >> "$CADDY_RECORD"
if [ "${1:-}" = validate ] && [ "${FAKE_CADDY_VALIDATE_FAIL:-0}" = 1 ]; then
    exit 1
fi
exit 0
FIXTURE

cat > "$FIXTURE_DIR/bun" <<'FIXTURE'
#!/usr/bin/env bash
{
    printf 'NEXUS_CLOUD_API_KEY=%s\n' "${NEXUS_CLOUD_API_KEY-<unset>}"
    printf 'CF_API_TOKEN=%s\n' "${CF_API_TOKEN-<unset>}"
    printf 'NEXUS_STORAGE_S3_ACCESS_KEY=%s\n' "${NEXUS_STORAGE_S3_ACCESS_KEY-<unset>}"
    printf 'NEXUS_STORAGE_S3_SECRET_KEY=%s\n' "${NEXUS_STORAGE_S3_SECRET_KEY-<unset>}"
    printf 'NEXUS__STORAGE__ACCESS_KEY=%s\n' "${NEXUS__STORAGE__ACCESS_KEY-<unset>}"
    printf 'NEXUS__STORAGE__SECRET_KEY=%s\n' "${NEXUS__STORAGE__SECRET_KEY-<unset>}"
} > "$CLOUD_ENV_RECORD"
FIXTURE

cat > "$FIXTURE_DIR/delayed-start-candidate" <<'FIXTURE'
#!/usr/bin/env bash
mkdir -p "$PROC_ROOT/$$"
printf '%s\0' bun run src/index.ts > "$PROC_ROOT/$$/cmdline"
printf '%s\n' "$$" > "$DELAYED_LISTENER_PID_FILE"
sleep 5
FIXTURE

chmod +x "$FIXTURE_DIR/ss" "$FIXTURE_DIR/readlink" "$FIXTURE_DIR/curl" \
    "$FIXTURE_DIR/kill" "$FIXTURE_DIR/caddy" "$FIXTURE_DIR/bun" \
    "$FIXTURE_DIR/delayed-start-candidate"

export SS_BIN="$FIXTURE_DIR/ss"
export READLINK_BIN="$FIXTURE_DIR/readlink"
export PID_DIR
export PROC_ROOT
export CURL_BIN="$FIXTURE_DIR/curl"
export KILL_BIN="$FIXTURE_DIR/kill"
export CADDY_BIN="$FIXTURE_DIR/caddy"
export CURL_RECORD="$TEST_ROOT/curl.argv"
export KILL_RECORD="$TEST_ROOT/kill.argv"
export CADDY_RECORD="$TEST_ROOT/caddy.argv"
export CLOUD_ENV_RECORD="$TEST_ROOT/cloud.environment"

# The process under test must exist for kill -0, but all listener, cwd and
# command-line observations come from fixtures. The tests never inspect or
# write the production PID directory.
TEST_PID="$$"
LISTENER_LINE="LISTEN 0 511 127.0.0.1:4310 0.0.0.0:* users:((\"bun\",pid=$TEST_PID,fd=7))"
mkdir -p "$PROC_ROOT/$TEST_PID"

# shellcheck source=/dev/null
source "$PRODUCTION_DIR/processes.sh"

export NEXUS_PRODUCTION_LOG_DIR="$TEST_ROOT/logs"
export NEXUS_PRODUCTION_PID_DIR="$PID_DIR"
# shellcheck source=/dev/null
source "$PRODUCTION_DIR/deploy.sh"

cat > "$FIXTURE_DIR/start-candidate" <<'FIXTURE'
#!/usr/bin/env bash
printf launched > "$START_MARKER"
FIXTURE
chmod +x "$FIXTURE_DIR/start-candidate"

failures=0

assert_equals() {
    local expected=$1
    local actual=$2
    local message=$3

    if [ "$expected" != "$actual" ]; then
        printf 'FAIL: %s (expected %q, got %q)\n' "$message" "$expected" "$actual" >&2
        return 1
    fi
}

assert_absent() {
    local path=$1
    local message=$2

    if [ -e "$path" ]; then
        printf 'FAIL: %s (%s exists)\n' "$message" "$path" >&2
        return 1
    fi
}

assert_contains() {
    local needle=$1
    local haystack=$2
    local message=$3

    case "$haystack" in
        *"$needle"*) ;;
        *)
            printf 'FAIL: %s (missing %q)\n' "$message" "$needle" >&2
            return 1
            ;;
    esac
}

write_cmdline() {
    printf '%s\0' "$@" > "$PROC_ROOT/$TEST_PID/cmdline"
}

reset_fixtures() {
    rm -f "$PID_DIR"/*.pid
    rm -f "$CURL_RECORD" "$KILL_RECORD" "$CADDY_RECORD" "$CLOUD_ENV_RECORD"
    export SS_FAIL=0
    export SS_OUTPUT="$LISTENER_LINE"
    export READLINK_OUTPUT="$SERVICE_DIR"
    export FAKE_CADDY_VALIDATE_FAIL=0
    export FAKE_CURL_FAIL=0
    export FAKE_CURL_BODY='{"ok":true}'
    unset SS_PID_FILE SS_REQUIRE_PID_FILE
    write_cmdline bun run src/index.ts
}

test_empty_listener_state_is_absent() {
    reset_fixtures
    export SS_OUTPUT=""

    assert_equals absent "$(listener_state 4310 2>/dev/null)" \
        'an inspected port with no listener is not unverifiable'
}

test_start_appends_to_existing_service_log() {
    reset_fixtures
    export SS_OUTPUT=""
    export START_MARKER="$TEST_ROOT/started"
    printf 'historical-secret-sentinel\n' > "$NEXUS_PRODUCTION_LOG_DIR/auth.log"
    NEXUS_PRODUCTION_START_SETTLE_SECONDS=0

    if start_service auth "$ROOT/apps/Nexus-Auth" 4310 "$FIXTURE_DIR/start-candidate"; then
        printf 'FAIL: non-listening start candidate was accepted\n' >&2
        return 1
    fi

    assert_contains 'historical-secret-sentinel' "$(<"$NEXUS_PRODUCTION_LOG_DIR/auth.log")" \
        'service start truncated the historical log before invalidation cleanup'
}

test_stop_refuses_a_mismatched_pid_without_signalling_it() {
    reset_fixtures
    declare -F stop_service >/dev/null || {
        printf 'FAIL: per-service stop path is missing\n' >&2
        return 1
    }
    printf '%s\n' "$TEST_PID" > "$PID_DIR/auth.pid"
    write_cmdline python3 foreign-service.py

    if stop_service auth >/dev/null 2>&1; then
        printf 'FAIL: stop accepted a PID whose process identity mismatched\n' >&2
        return 1
    fi

    assert_absent "$KILL_RECORD" 'mismatched PID was sent a signal' &&
        [ -f "$PID_DIR/auth.pid" ]
}

test_http_health_requires_curl_fail_and_semantic_success() {
    reset_fixtures
    declare -F http_endpoint_healthy >/dev/null || {
        printf 'FAIL: semantic HTTP health helper is missing\n' >&2
        return 1
    }
    export FAKE_CURL_BODY='{"ok":false}'
    if http_endpoint_healthy https://service.example.test/health; then
        printf 'FAIL: {"ok":false} was accepted as healthy\n' >&2
        return 1
    fi
    export FAKE_CURL_BODY='{"ok":true}'
    http_endpoint_healthy https://service.example.test/health || {
        printf 'FAIL: {"ok":true} was not accepted as healthy\n' >&2
        return 1
    }

    assert_contains '--fail' "$(<"$CURL_RECORD")" 'HTTP health probe did not enable curl HTTP-status failure handling'
}

test_caddy_restart_validates_config_before_stopping() {
    reset_fixtures
    declare -F cmd_restart_service >/dev/null || {
        printf 'FAIL: safe per-service restart path is missing\n' >&2
        return 1
    }
    export SS_OUTPUT="LISTEN 0 511 127.0.0.1:8095 0.0.0.0:* users:((\"caddy\",pid=$TEST_PID,fd=7))"
    export READLINK_OUTPUT="$ROOT"
    write_cmdline caddy run --config "$ROOT/deploy/production/nexus-chat.Caddyfile" --adapter caddyfile
    printf '%s\n' "$TEST_PID" > "$PID_DIR/nexus-chat-web.pid"
    export FAKE_CADDY_VALIDATE_FAIL=1

    if cmd_restart_service nexus-chat-web >/dev/null 2>&1; then
        printf 'FAIL: Caddy restart proceeded after config validation failed\n' >&2
        return 1
    fi

    assert_absent "$KILL_RECORD" 'Caddy was stopped before its replacement config validated'
}

test_cloud_restart_refuses_a_mismatched_managed_pid() {
    reset_fixtures
    export SS_OUTPUT="LISTEN 0 511 127.0.0.1:8787 0.0.0.0:* users:((\"bun\",pid=$TEST_PID,fd=7))"
    export READLINK_OUTPUT="$ROOT/apps/Nexus-Cloud"
    export NEXUS_CLOUD_API_KEY=NON_SECRET_TEST_KEY
    write_cmdline python3 foreign-cloud.py
    printf '%s\n' "$TEST_PID" > "$PID_DIR/cloud.pid"

    if cmd_restart_service cloud >/dev/null 2>&1; then
        printf 'FAIL: Cloud restart accepted a mismatched managed PID\n' >&2
        return 1
    fi

    assert_absent "$KILL_RECORD" 'Cloud restart signalled a mismatched PID'
}

test_cloud_launch_loads_protected_env_without_secret_argv_or_stale_inheritance() {
    (
        reset_fixtures
        local cloud_env capture attempt
        cloud_env="$TEST_ROOT/cloud-launch.env"
        capture="$TEST_ROOT/cloud-launch.argv"
        cat > "$cloud_env" <<'CLOUD_ENV'
NEXUS_CLOUD_API_KEY=FILE_CLOUD_KEY_SENTINEL
CF_API_TOKEN=FILE_CF_TOKEN_SENTINEL
NEXUS_STORAGE_S3_ACCESS_KEY=FILE_STORAGE_ACCESS_SENTINEL
NEXUS_STORAGE_S3_SECRET_KEY=FILE_STORAGE_SECRET_SENTINEL
CLOUD_ENV
        chmod 600 "$cloud_env"
        export NEXUS_CLOUD_ENV_FILE="$cloud_env"
        export NEXUS_CLOUD_BINARY_PATH="$FIXTURE_DIR/bun"
        export NEXUS_CLOUD_API_KEY=STALE_CLOUD_KEY_SENTINEL
        export CF_API_TOKEN=STALE_CF_TOKEN_SENTINEL
        export NEXUS_STORAGE_S3_ACCESS_KEY=STALE_STORAGE_ACCESS_SENTINEL
        export NEXUS_STORAGE_S3_SECRET_KEY=STALE_STORAGE_SECRET_SENTINEL
        export NEXUS__STORAGE__ACCESS_KEY=STALE_ALIAS_ACCESS_SENTINEL
        export NEXUS__STORAGE__SECRET_KEY=STALE_ALIAS_SECRET_SENTINEL

        start_service() {
            printf '%s\n' "$@" > "$capture"
        }
        start_cloud_service

        for secret in \
            FILE_CLOUD_KEY_SENTINEL FILE_CF_TOKEN_SENTINEL \
            FILE_STORAGE_ACCESS_SENTINEL FILE_STORAGE_SECRET_SENTINEL \
            STALE_CLOUD_KEY_SENTINEL STALE_CF_TOKEN_SENTINEL \
            STALE_STORAGE_ACCESS_SENTINEL STALE_STORAGE_SECRET_SENTINEL \
            STALE_ALIAS_ACCESS_SENTINEL STALE_ALIAS_SECRET_SENTINEL
        do
            if grep -Fq -- "$secret" "$capture"; then
                printf 'FAIL: Cloud launch exposed a secret in launcher argv\n' >&2
                return 1
            fi
        done

        # Exercise the protected wrapper itself. It must replace the inherited
        # stale values and remove aliases that are absent from the file.
        PATH="$FIXTURE_DIR:$PATH" bash "$PRODUCTION_DIR/start-cloud.sh"
        for ((attempt = 1; attempt <= 20; attempt++)); do
            [ -f "$CLOUD_ENV_RECORD" ] && break
            sleep 0.01
        done
        assert_contains 'NEXUS_CLOUD_API_KEY=FILE_CLOUD_KEY_SENTINEL' "$(<"$CLOUD_ENV_RECORD")" \
            'Cloud wrapper did not load the protected API key' || return 1
        assert_contains 'NEXUS_STORAGE_S3_ACCESS_KEY=FILE_STORAGE_ACCESS_SENTINEL' "$(<"$CLOUD_ENV_RECORD")" \
            'Cloud wrapper did not load the protected storage access key' || return 1
        assert_contains 'NEXUS__STORAGE__ACCESS_KEY=<unset>' "$(<"$CLOUD_ENV_RECORD")" \
            'Cloud wrapper retained a stale storage access-key alias' || return 1
        assert_contains 'NEXUS__STORAGE__SECRET_KEY=<unset>' "$(<"$CLOUD_ENV_RECORD")" \
            'Cloud wrapper retained a stale storage secret-key alias'
    )
}

test_nexus_chat_restart_uses_the_validated_service_path() {
    (
        reset_fixtures
        local chat_env chat_binary
        chat_env="$TEST_ROOT/nexus-chat.restart.env"
        chat_binary="$TEST_ROOT/nexus-chat.binary"
        cat > "$chat_env" <<'CHAT_ENV'
NEXUS__STORAGE__ENDPOINT=http://127.0.0.1:9000
NEXUS__STORAGE__ACCESS_KEY=CHAT_ACCESS_SENTINEL
NEXUS__STORAGE__SECRET_KEY=CHAT_SECRET_SENTINEL
NEXUS__STORAGE__BUCKET=nexus-chat-uploads
CHAT_ENV
        printf '#!/usr/bin/env bash\nexit 0\n' > "$chat_binary"
        chmod 600 "$chat_env"
        chmod 700 "$chat_binary"
        export NEXUS_CHAT_ENV_FILE="$chat_env"
        export NEXUS_CHAT_BINARY_PATH="$chat_binary"
        export SS_OUTPUT="LISTEN 0 511 127.0.0.1:8180 0.0.0.0:* users:((\"nexus\",pid=$TEST_PID,fd=7))"
        export READLINK_OUTPUT="$ROOT/apps/Nexus"
        write_cmdline ./target/debug/nexus serve --port 8180 --gateway-port 8181 --voice-port 8182
        printf '%s\n' "$TEST_PID" > "$PID_DIR/nexus-chat.pid"
        export FAKE_KILL_STATUS=1

        if cmd_restart_service nexus-chat >/dev/null 2>&1; then
            printf 'FAIL: fake nexus-chat stop unexpectedly succeeded\n' >&2
            return 1
        fi
        assert_contains "$TEST_PID" "$(<"$KILL_RECORD")" \
            'nexus-chat restart did not reach the validated per-service stop path'
    )
}

test_start_reconciles_a_delayed_exact_listener() {
    (
        reset_fixtures
        local managed_pid
        export DELAYED_LISTENER_PID_FILE="$TEST_ROOT/delayed-listener.pid"
        export SS_REQUIRE_PID_FILE=1
        export SS_PID_FILE="$DELAYED_LISTENER_PID_FILE"
        export SS_OUTPUT='LISTEN 0 511 127.0.0.1:4310 0.0.0.0:* users:(("bun",pid=__PID__,fd=7))'
        export READLINK_OUTPUT="$ROOT/apps/Nexus-Auth"
        export NEXUS_PRODUCTION_START_ATTEMPTS=30
        export NEXUS_PRODUCTION_START_INTERVAL=0.02

        start_service auth "$ROOT/apps/Nexus-Auth" 4310 "$FIXTURE_DIR/delayed-start-candidate" \
            >/dev/null
        IFS= read -r managed_pid < "$PID_DIR/auth.pid"
        assert_equals "$(<"$DELAYED_LISTENER_PID_FILE")" "$managed_pid" \
            'delayed exact listener was not adopted into its PID file' || return 1
        kill "$managed_pid" 2>/dev/null || true
    )
}

test_stop_recovers_an_exact_late_listener_without_a_pid_file() {
    reset_fixtures
    export SS_OUTPUT="$LISTENER_LINE"
    export READLINK_OUTPUT="$ROOT/apps/Nexus-Auth"
    write_cmdline bun run src/index.ts
    rm -f "$PID_DIR/auth.pid"
    export FAKE_KILL_STATUS=1

    if stop_service auth >/dev/null 2>&1; then
        printf 'FAIL: fake late-listener stop unexpectedly succeeded\n' >&2
        return 1
    fi
    assert_contains "$TEST_PID" "$(<"$KILL_RECORD")" \
        'exact late listener was not reconciled before the recovery stop'
}

test_sensitive_log_reset_requires_both_rotation_checkpoints() {
    reset_fixtures
    declare -F reset_sensitive_log_after_checkpoints >/dev/null || {
        printf 'FAIL: checkpoint-gated sensitive-log reset is missing\n' >&2
        return 1
    }
    local runtime rotation log_file
    runtime="$TEST_ROOT/rotation-runtime"
    rotation="$runtime/rotation.ABCDEF"
    log_file="$NEXUS_PRODUCTION_LOG_DIR/cloud.log"
    mkdir -p "$rotation"
    printf '%s\n' "$rotation" > "$runtime/rotation.current"
    printf 'credential-bearing-history\n' > "$log_file"
    export NEXUS_ROTATION_RUNTIME_DIR="$runtime"

    if reset_sensitive_log_after_checkpoints cloud >/dev/null 2>&1; then
        printf 'FAIL: sensitive log reset succeeded without rotation checkpoints\n' >&2
        return 1
    fi
    assert_contains credential-bearing-history "$(<"$log_file")" \
        'missing checkpoints did not preserve the historical log' || return 1

    /usr/bin/install -m 600 /dev/null "$rotation/tunnel.checkpoint"
    /usr/bin/install -m 600 /dev/null "$rotation/storage.checkpoint"
    reset_sensitive_log_after_checkpoints cloud || return 1
    assert_equals '' "$(<"$log_file")" 'checkpointed sensitive log reset did not install an empty log'
}

test_adopts_one_matching_listener() {
    reset_fixtures

    local actual
    actual="$(reconcile_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts")"

    assert_equals "$TEST_PID" "$actual" "matching listener is adopted" &&
        assert_equals "$TEST_PID" "$(<"$PID_DIR/auth.pid")" "adopted PID is persisted"
}

test_rejects_foreign_listener() {
    reset_fixtures
    write_cmdline python3 foreign-service.py

    if reconcile_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts" >/dev/null 2>&1; then
        printf 'FAIL: foreign listener was adopted\n' >&2
        return 1
    fi

    assert_absent "$PID_DIR/auth.pid" "foreign listener does not create a PID file"
}

test_replaces_stale_pid_file() {
    reset_fixtures
    printf '999999\n' > "$PID_DIR/auth.pid"

    local actual
    actual="$(reconcile_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts")"

    assert_equals "$TEST_PID" "$actual" "live owned listener replaces stale PID" &&
        assert_equals "$TEST_PID" "$(<"$PID_DIR/auth.pid")" "stale PID file is replaced"
}

test_rejects_multiple_listener_pids() {
    reset_fixtures
    export SS_OUTPUT="$LISTENER_LINE
LISTEN 0 511 127.0.0.1:4310 0.0.0.0:* users:((\"bun\",pid=424242,fd=8))"

    if reconcile_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts" >/dev/null 2>&1; then
        printf 'FAIL: multiple listener PIDs were adopted\n' >&2
        return 1
    fi

    assert_absent "$PID_DIR/auth.pid" "ambiguous listeners do not create a PID file"
}

test_validated_pid_rejects_pid_that_no_longer_owns_port() {
    reset_fixtures
    printf '%s\n' "$TEST_PID" > "$PID_DIR/auth.pid"
    export SS_OUTPUT=""

    if validated_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts" >/dev/null 2>&1; then
        printf 'FAIL: PID without the listener was accepted\n' >&2
        return 1
    fi
}

test_rejects_listener_with_a_foreign_proc_cmdline() {
    reset_fixtures
    # The ownership check reads the NUL-delimited kernel command line from the
    # proc seam rather than a formatted process-listing command.
    write_cmdline python3 foreign-service.py

    if reconcile_pid auth 4310 "$SERVICE_DIR" "bun run src/index.ts" >/dev/null 2>&1; then
        printf 'FAIL: listener with a foreign proc cmdline was adopted\n' >&2
        return 1
    fi

    assert_absent "$PID_DIR/auth.pid" "foreign proc cmdline does not create a PID file"
}

test_ss_inspection_failure_is_unverifiable() {
    reset_fixtures
    export SS_FAIL=1

    local actual
    actual="$(listener_state 4310 2>/dev/null)"
    assert_equals "unverifiable" "$actual" "ss inspection failure is not treated as an empty port"
}

test_start_refuses_ss_inspection_failure() {
    reset_fixtures
    export SS_FAIL=1
    export START_MARKER="$TEST_ROOT/started"
    rm -f "$START_MARKER"

    local output
    if output="$(start_service auth "$ROOT/apps/Nexus-Auth" 4310 "$FIXTURE_DIR/start-candidate" 2>&1)"; then
        printf 'FAIL: start proceeded after listener inspection failed\n' >&2
        return 1
    fi

    assert_contains "cannot inspect :4310 safely — refusing to start" "$output" "start reports inspection failure" &&
        assert_absent "$START_MARKER" "start does not launch after an inspection failure"
}

test_status_reports_ss_inspection_failure_as_unverifiable() {
    reset_fixtures
    export SS_FAIL=1

    local output
    output="$(cmd_service_status 2>&1)"
    assert_contains "auth" "$output" "status includes auth" &&
        assert_contains "unverifiable listener state on :4310" "$output" "status does not report an inspection failure as not running"
}

run_test() {
    local name=$1

    if "$name"; then
        printf 'ok - %s\n' "$name"
    else
        failures=$((failures + 1))
    fi
}

run_test test_adopts_one_matching_listener
run_test test_rejects_foreign_listener
run_test test_replaces_stale_pid_file
run_test test_rejects_multiple_listener_pids
run_test test_validated_pid_rejects_pid_that_no_longer_owns_port
run_test test_rejects_listener_with_a_foreign_proc_cmdline
run_test test_ss_inspection_failure_is_unverifiable
run_test test_start_refuses_ss_inspection_failure
run_test test_status_reports_ss_inspection_failure_as_unverifiable
run_test test_empty_listener_state_is_absent
run_test test_start_appends_to_existing_service_log
run_test test_stop_refuses_a_mismatched_pid_without_signalling_it
run_test test_http_health_requires_curl_fail_and_semantic_success
run_test test_caddy_restart_validates_config_before_stopping
run_test test_cloud_restart_refuses_a_mismatched_managed_pid
run_test test_sensitive_log_reset_requires_both_rotation_checkpoints
run_test test_cloud_launch_loads_protected_env_without_secret_argv_or_stale_inheritance
run_test test_nexus_chat_restart_uses_the_validated_service_path
run_test test_start_reconciles_a_delayed_exact_listener
run_test test_stop_recovers_an_exact_late_listener_without_a_pid_file

if [ "$failures" -ne 0 ]; then
    printf '%d process ownership test(s) failed\n' "$failures" >&2
    exit 1
fi

printf 'all process ownership tests passed\n'
