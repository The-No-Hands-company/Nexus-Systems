#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCTION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-processes-test.XXXXXX")"
FIXTURE_DIR="$TEST_ROOT/fixtures"
SERVICE_DIR="$TEST_ROOT/service"
PID_DIR="$TEST_ROOT/pids"
mkdir -p "$FIXTURE_DIR" "$SERVICE_DIR" "$PID_DIR"

cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT

cat > "$FIXTURE_DIR/ss" <<'FIXTURE'
#!/usr/bin/env bash
printf '%s\n' "${SS_OUTPUT:-}"
FIXTURE

cat > "$FIXTURE_DIR/ps" <<'FIXTURE'
#!/usr/bin/env bash
printf '%s\n' "${PS_OUTPUT:-}"
FIXTURE

cat > "$FIXTURE_DIR/readlink" <<'FIXTURE'
#!/usr/bin/env bash
printf '%s\n' "${READLINK_OUTPUT:-}"
FIXTURE

chmod +x "$FIXTURE_DIR/ss" "$FIXTURE_DIR/ps" "$FIXTURE_DIR/readlink"

export SS_BIN="$FIXTURE_DIR/ss"
export PS_BIN="$FIXTURE_DIR/ps"
export READLINK_BIN="$FIXTURE_DIR/readlink"
export PID_DIR

# The process under test must exist for kill -0, but all listener, cwd and
# command-line observations come from fixtures. The tests never inspect or
# write the production PID directory.
TEST_PID="$$"
LISTENER_LINE="LISTEN 0 511 127.0.0.1:4310 0.0.0.0:* users:((\"bun\",pid=$TEST_PID,fd=7))"

# shellcheck source=/dev/null
source "$PRODUCTION_DIR/processes.sh"

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

reset_fixtures() {
    rm -f "$PID_DIR"/*.pid
    export SS_OUTPUT="$LISTENER_LINE"
    export PS_OUTPUT="bun run src/index.ts"
    export READLINK_OUTPUT="$SERVICE_DIR"
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
    export PS_OUTPUT="python3 foreign-service.py"

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

if [ "$failures" -ne 0 ]; then
    printf '%d process ownership test(s) failed\n' "$failures" >&2
    exit 1
fi

printf 'all process ownership tests passed\n'
