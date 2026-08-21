#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCTION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-terminal-production-test.XXXXXX")"
FIXTURE_ROOT="$TEST_ROOT/workspace"
START_RECORD="$TEST_ROOT/start-service.args"
CURL_RECORD="$TEST_ROOT/curl.args"

cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT

mkdir -p \
    "$FIXTURE_ROOT/apps/Nexus-Dashboard/frontend/dist" \
    "$FIXTURE_ROOT/apps/Nexus-Dashboard/src" \
    "$FIXTURE_ROOT/packages/phantom-sdk/wasm/target/release"
touch "$FIXTURE_ROOT/apps/Nexus-Dashboard/frontend/dist/index.html"
touch "$FIXTURE_ROOT/packages/phantom-sdk/wasm/target/release/libphantom_wasm.so"

# deploy.sh is a command as well as a function library. Source the real function
# bodies while replacing only the top-level log directory and omitting command
# dispatch, so this test cannot start or stop production services.
export NEXUS_PRODUCTION_LOG_DIR="$TEST_ROOT/logs"
# shellcheck source=/dev/null
source <(
    sed \
        -e 's#^LOG_DIR="/tmp/nexus-production"#LOG_DIR="${NEXUS_PRODUCTION_LOG_DIR:-/tmp/nexus-production}"#' \
        -e '/^case "${1:-}" in$/,$d' \
        "$PRODUCTION_DIR/deploy.sh"
)

ROOT="$FIXTURE_ROOT"
export NEXUS_CLOUD_API_KEY="task-7-test-cloud-key" # pragma: allowlist secret
export NEXUS_ISSUES_TOKEN=""

# Capture the behavior of cmd_start at its service-launch boundary. Everything
# outside that boundary is inert and local to TEST_ROOT.
start_service() {
    printf '%s' "$1" >> "$START_RECORD"
    shift
    printf '\t%s' "$@" >> "$START_RECORD"
    printf '\n' >> "$START_RECORD"
}
docker-compose() { :; }
sleep() { :; }
curl() {
    printf '%s\n' "$*" >> "$CURL_RECORD"
    printf '%s\n' '{"status":"ok"}'
}

failures=0

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    failures=$((failures + 1))
}

assert_contains() {
    local needle=$1
    local haystack=$2
    local message=$3
    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$message (missing: $needle)" ;;
    esac
}

assert_service_order() {
    local first=$1
    local second=$2
    local first_line second_line
    first_line="$(awk -F '\t' -v service="$first" '$1 == service { print NR; exit }' "$START_RECORD")"
    second_line="$(awk -F '\t' -v service="$second" '$1 == service { print NR; exit }' "$START_RECORD")"
    if [ -z "$first_line" ] || [ -z "$second_line" ] || [ "$first_line" -ge "$second_line" ]; then
        fail "$first must start before $second"
    fi
}

service_args() {
    local service=$1
    awk -F '\t' -v service="$service" '$1 == service { print; exit }' "$START_RECORD"
}

run_start() {
    : > "$START_RECORD"
    : > "$CURL_RECORD"
    cmd_start >/dev/null
}

unset NEXUS_TERMINAL_ENABLED
run_start

terminal_args="$(service_args terminal)"
dashboard_args="$(service_args dashboard)"

assert_service_order terminal dashboard
assert_contains $'terminal\t' "$terminal_args" "production launch did not start Nexus-Terminal"
assert_contains $'\t3110\t' "$terminal_args" "Nexus-Terminal did not use port 3110"
assert_contains $'\tPORT=3110' "$terminal_args" "Nexus-Terminal did not receive its port"
assert_contains $'\tNEXUS_BIND_HOST=127.0.0.1' "$terminal_args" "Nexus-Terminal was not bound to loopback"
assert_contains $'\tNEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310' "$terminal_args" "Nexus-Terminal did not receive the loopback Auth URL"
assert_contains $'\tNEXUS_CLOUD_URL=http://127.0.0.1:8787' "$terminal_args" "Nexus-Terminal did not receive the loopback Cloud URL"
assert_contains $'\tNEXUS_CLOUD_API_KEY=task-7-test-cloud-key' "$terminal_args" "Nexus-Terminal did not receive the Cloud API key" # pragma: allowlist secret
assert_contains $'\tNEXUS_NEXUS_TERMINAL_BASE_URL=http://127.0.0.1:3110' "$terminal_args" "Nexus-Terminal did not register its loopback base URL"
assert_contains $'\tNEXUS_TERMINAL_ENABLED=false' "$terminal_args" "Nexus-Terminal was not explicitly disabled by default"
assert_contains $'\tNEXUS_TERMINAL_URL=http://127.0.0.1:3110' "$dashboard_args" "Dashboard did not receive the loopback Terminal URL"
assert_contains 'http://127.0.0.1:3110/health' "$(<"$CURL_RECORD")" "production status did not health-check Nexus-Terminal"

export NEXUS_TERMINAL_ENABLED=true
run_start
assert_contains $'\tNEXUS_TERMINAL_ENABLED=true' "$(service_args terminal)" "production launch did not preserve an explicit Terminal enable switch"

if [ "$failures" -ne 0 ]; then
    printf '%d production process assertion(s) failed\n' "$failures" >&2
    exit 1
fi

printf 'PASS: production starts loopback Nexus-Terminal before Dashboard with an explicit safe-default enable switch\n'
