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
    "$FIXTURE_ROOT/apps/Nexus-Calendar/frontend/dist" \
    "$FIXTURE_ROOT/packages/phantom-sdk/wasm/target/release"
touch "$FIXTURE_ROOT/apps/Nexus-Dashboard/frontend/dist/index.html"
touch "$FIXTURE_ROOT/apps/Nexus-Calendar/frontend/dist/index.html"
touch "$FIXTURE_ROOT/apps/Nexus-Calendar/package.json"
touch "$FIXTURE_ROOT/packages/phantom-sdk/wasm/target/release/libphantom_wasm.so"

# deploy.sh is a command as well as a function library. Source the real function
# bodies while replacing only the top-level log directory and omitting command
# dispatch, so this test cannot start or stop production services.
export NEXUS_PRODUCTION_LOG_DIR="$TEST_ROOT/logs"
# Never touch the operator's real user timers from a test.
export NEXUS_SKIP_LOG_ROTATE_INSTALL=1 NEXUS_SKIP_CANARY_INSTALL=1
# The Calendar front door starts only when a Caddy binary exists; CI has none.
# start_service is recorded, not run, so any executable stands in for it.
export CADDY_BIN=true
# shellcheck source=/dev/null
source <(
    sed \
        -e 's#^LOG_DIR="/tmp/nexus-production"#LOG_DIR="${NEXUS_PRODUCTION_LOG_DIR:-/tmp/nexus-production}"#' \
        -e '/^case "${1:-}" in$/,$d' \
        -e "s#^source \"\$(cd \"\$(dirname \"\${BASH_SOURCE\[0\]}\")\" \&\& pwd)/processes.sh\"#source \"$PRODUCTION_DIR/processes.sh\"#" \
        "$PRODUCTION_DIR/deploy.sh"
)

ROOT="$FIXTURE_ROOT"
# Cloud's protected environment file is the only source of the Cloud key:
# cmd_start validates it, then exports the key for services to inherit.
export NEXUS_CLOUD_ENV_FILE="$TEST_ROOT/cloud.env"
cat > "$NEXUS_CLOUD_ENV_FILE" <<'CLOUD_ENV'
NEXUS_CLOUD_API_KEY=task-7-test-cloud-key
NEXUS_CLOUD_URL=http://127.0.0.1:8787
NEXUS_STORAGE_S3_ACCESS_KEY=test-access-sentinel
NEXUS_STORAGE_S3_SECRET_KEY=test-secret-sentinel
CLOUD_ENV
unset NEXUS_CLOUD_API_KEY
export NEXUS_ISSUES_TOKEN=""
export NEXUS_CALENDAR_DB="/var/lib/nexus-calendar/calendar.sqlite"
export NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT="usr-founder"
export NEXUS_CALENDAR_JWT_AUDIENCE="calendar.test.example"

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
proxy_args="$(service_args proxy)"
calendar_args="$(service_args calendar)"
calendar_web_args="$(service_args nexus-calendar-web)"

assert_service_order terminal dashboard
assert_contains $'terminal\t' "$terminal_args" "production launch did not start Nexus-Terminal"
assert_contains $'calendar\t' "$calendar_args" "production launch did not start Nexus-Calendar"
assert_contains $'nexus-calendar-web\t' "$calendar_web_args" "production launch did not start the Calendar web front door"
assert_contains $'\tNEXUS_CALENDAR_DB=/var/lib/nexus-calendar/calendar.sqlite' "$calendar_args" "Nexus-Calendar did not receive its persistent database path"
assert_contains $'\tNEXUS_CALENDAR_LEGACY_OWNER_SUBJECT=usr-founder' "$calendar_args" "Nexus-Calendar did not receive the explicit legacy owner" # pragma: allowlist secret
assert_contains $'\tNEXUS_CALENDAR_JWT_AUDIENCE=calendar.test.example' "$calendar_args" "Nexus-Calendar did not receive its JWT audience"
assert_contains $'\tNEXUS_CALENDAR_DASHBOARD_SECRET=' "$calendar_args" "Nexus-Calendar did not receive the Dashboard hop secret"
assert_contains $'\tNEXUS_CALENDAR_WEB_ROOT=' "$calendar_web_args" "Calendar web front door did not receive the shared built artifact root"
assert_contains $'\t3110\t' "$terminal_args" "Nexus-Terminal did not use port 3110"
assert_contains $'\tPORT=3110' "$terminal_args" "Nexus-Terminal did not receive its port"
assert_contains $'\tNEXUS_BIND_HOST=127.0.0.1' "$terminal_args" "Nexus-Terminal was not bound to loopback"
assert_contains $'\tNEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310' "$terminal_args" "Nexus-Terminal did not receive the loopback Auth URL"
assert_contains $'\tNEXUS_CLOUD_URL=http://127.0.0.1:8787' "$terminal_args" "Nexus-Terminal did not receive the loopback Cloud URL"
# The key reaches services by inheritance, never as a KEY=value launcher
# argument (which is visible in the process list while env execs).
if grep -q 'NEXUS_CLOUD_API_KEY=' "$START_RECORD"; then
    fail "a service received the Cloud API key as a launcher argument"
fi
[ "${NEXUS_CLOUD_API_KEY:-}" = "task-7-test-cloud-key" ] \
    || fail "cmd_start did not export the protected Cloud API key for services to inherit" # pragma: allowlist secret
assert_contains $'\tNEXUS_NEXUS_TERMINAL_BASE_URL=http://127.0.0.1:3110' "$terminal_args" "Nexus-Terminal did not register its loopback base URL"
assert_contains $'\tNEXUS_TERMINAL_ENABLED=false' "$terminal_args" "Nexus-Terminal was not explicitly disabled by default"
assert_contains $'\tNEXUS_TERMINAL_URL=http://127.0.0.1:3110' "$dashboard_args" "Dashboard did not receive the loopback Terminal URL"
assert_contains $'\tDASHBOARD_UPSTREAM=http://127.0.0.1:3132' "$proxy_args" "production proxy did not receive the fixed loopback Dashboard upstream"
assert_contains 'http://127.0.0.1:3110/health' "$(<"$CURL_RECORD")" "production status did not health-check Nexus-Terminal"

# The real start_service refuses any service without an ownership identity, so
# a service cmd_start launches but service_identity/service_port do not know
# would silently never start in production.
while IFS=$'\t' read -r service _; do
    [ -n "$service" ] || continue
    service_identity "$service" >/dev/null 2>&1 \
        || fail "cmd_start launches $service but service_identity does not know it"
    service_port "$service" >/dev/null 2>&1 \
        || fail "cmd_start launches $service but service_port does not know it"
done < "$START_RECORD"

# The fixture only launches services whose directories it creates, so also
# check statically: every service deploy.sh can launch must be known.
while IFS= read -r service; do
    service_identity "$service" >/dev/null 2>&1 \
        || fail "deploy.sh can launch $service but service_identity does not know it"
    service_port "$service" >/dev/null 2>&1 \
        || fail "deploy.sh can launch $service but service_port does not know it"
done < <(grep -o 'start_service "[a-z-]*"' "$PRODUCTION_DIR/deploy.sh" | cut -d'"' -f2 | sort -u)

export NEXUS_TERMINAL_ENABLED=true
run_start
assert_contains $'\tNEXUS_TERMINAL_ENABLED=true' "$(service_args terminal)" "production launch did not preserve an explicit Terminal enable switch"

if [ "$failures" -ne 0 ]; then
    printf '%d production process assertion(s) failed\n' "$failures" >&2
    exit 1
fi

printf 'PASS: production starts loopback Nexus-Terminal before Dashboard with an explicit safe-default enable switch\n'
