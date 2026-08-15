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
printf '%s\n' "${SS_OUTPUT:-}"
FIXTURE

cat > "$FIXTURE_DIR/readlink" <<'FIXTURE'
#!/usr/bin/env bash
printf '%s\n' "${READLINK_OUTPUT:-}"
FIXTURE

chmod +x "$FIXTURE_DIR/ss" "$FIXTURE_DIR/readlink"

export SS_BIN="$FIXTURE_DIR/ss"
export READLINK_BIN="$FIXTURE_DIR/readlink"
export PID_DIR
export PROC_ROOT

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
    export SS_FAIL=0
    export SS_OUTPUT="$LISTENER_LINE"
    export READLINK_OUTPUT="$SERVICE_DIR"
    write_cmdline bun run src/index.ts
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

if [ "$failures" -ne 0 ]; then
    printf '%d process ownership test(s) failed\n' "$failures" >&2
    exit 1
fi

printf 'all process ownership tests passed\n'
