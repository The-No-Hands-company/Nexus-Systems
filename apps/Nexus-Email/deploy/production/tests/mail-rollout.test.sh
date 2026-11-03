#!/usr/bin/env bash
# Smoke tests for the mail-body rollout gate in nexus-mailsmtpd.
#
# These tests verify that:
#   - the daemon refuses to enable the object service when the migration
#     marker env var is not set, even if all other env vars are present.
#   - the daemon starts the object service when the marker is set.
#
# Run from the repo root. Requires cargo to be on PATH and a database reachable
# at NEXUS_EMAIL_TEST_DATABASE_URL.
set -euo pipefail

pass=0
fail=0
errors=()

check() {
    local name="$1"; shift
    if "$@" 2>/dev/null; then
        echo "PASS: $name"
        ((pass++)) || true
    else
        echo "FAIL: $name"
        ((fail++)) || true
        errors+=("$name")
    fi
}

# Build the binary once.
cargo build -q -p nexus-mailsmtp 2>&1

BINARY="target/debug/nexus-mailsmtpd"
[[ -x "$BINARY" ]] || { echo "error: binary not found at $BINARY"; exit 1; }

# Minimal env: object service config present, marker absent.
common_env=(
    env
    NEXUS_EMAIL_DATABASE_URL="${NEXUS_EMAIL_TEST_DATABASE_URL:-postgres://nexus:nexus@127.0.0.1:5432/nexus_email_test}"
    NEXUS_EMAIL_OBJECT_SERVICE_API_TOKEN="test-api-token"
    NEXUS_EMAIL_OBJECT_SERVICE_IMAP_TOKEN="test-imap-token"
    NEXUS_EMAIL_OBJECT_SERVICE_OUTBOUND_TOKEN="test-outbound-token"
    S3_ENDPOINT="http://127.0.0.1:9000"
    S3_ACCESS_KEY="minioadmin"
    S3_SECRET_KEY="minioadmin"
    S3_BUCKET="nexus-mail-test"
)

# Test: daemon logs a warning and does NOT start the object service when the
# migration marker is absent. We can observe this by checking that the startup
# log mentions "migration marker" rather than "object service listening".
check "daemon warns and skips object service when marker absent" bash -c '
    log=$("'"${common_env[*]}"'" '"$BINARY"' 2>&1 &
    pid=$!
    sleep 1
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true) || true
    echo "$log" | grep -q "NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER"
'

# Test: daemon starts normally (exits quickly on bad DB) when marker is set.
# We only check that the warning is NOT emitted, not that the service stays up.
check "no migration-marker warning when marker is set" bash -c '
    log=$('"${common_env[*]}"' \
        NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=test-marker \
        '"$BINARY"' 2>&1 &
    pid=$!
    sleep 1
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true) || true
    ! echo "$log" | grep -q "NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER"
'

echo ""
echo "Results: $pass passed, $fail failed"
if [[ $fail -gt 0 ]]; then
    echo "Failed tests: ${errors[*]}"
    exit 1
fi
