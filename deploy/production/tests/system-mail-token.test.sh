#!/usr/bin/env bash
# The shared system-mail token: generated once, kept in Nexus-Email's .env with
# owner-only permissions, never replaced, never printed.
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCTION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-system-mail-token-test.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

export NEXUS_PRODUCTION_LOG_DIR="$TEST_ROOT/logs"
# shellcheck source=/dev/null
source <(
    sed \
        -e 's#^LOG_DIR="/tmp/nexus-production"#LOG_DIR="${NEXUS_PRODUCTION_LOG_DIR:-/tmp/nexus-production}"#' \
        -e '/^case "${1:-}" in$/,$d' \
        -e "s#^source \"\$(cd \"\$(dirname \"\${BASH_SOURCE\[0\]}\")\" \&\& pwd)/processes.sh\"#source \"$PRODUCTION_DIR/processes.sh\"#" \
        "$PRODUCTION_DIR/deploy.sh"
)

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

ROOT="$TEST_ROOT/workspace"
mkdir -p "$ROOT/apps/Nexus-Email"
ENV_FILE="$ROOT/apps/Nexus-Email/.env"
printf 'NEXUS_EMAIL_EGRESS=resend\n' > "$ENV_FILE"

unset NEXUS_EMAIL_SYSTEM_MAIL_TOKEN
# Called directly, not in $(...): a subshell would drop the variable it sets.
ensure_system_mail_token >"$TEST_ROOT/out" 2>&1
output="$(cat "$TEST_ROOT/out")"
token="$(sed -n 's/^NEXUS_EMAIL_SYSTEM_MAIL_TOKEN=//p' "$ENV_FILE")"
[[ "$token" =~ ^[0-9a-f]{64}$ ]] || fail "a 64-hex token is written to Nexus-Email's .env"
[ "${NEXUS_EMAIL_SYSTEM_MAIL_TOKEN:-}" = "$token" ] || fail "the token is set in the deploy shell"
[[ "$output" != *"$token"* ]] || fail "the token is never printed"
[ "$(stat -c '%a' "$ENV_FILE")" = "600" ] || fail ".env is owner-only after the token is written"
grep -q '^NEXUS_EMAIL_EGRESS=resend$' "$ENV_FILE" || fail "existing settings are kept"

unset NEXUS_EMAIL_SYSTEM_MAIL_TOKEN
ensure_system_mail_token >/dev/null 2>&1
[ "${NEXUS_EMAIL_SYSTEM_MAIL_TOKEN:-}" = "$token" ] || fail "a second run adopts the stored token"
[ "$(grep -c '^NEXUS_EMAIL_SYSTEM_MAIL_TOKEN=' "$ENV_FILE")" = "1" ] || fail "a second run does not add another line"

rm -rf "$ROOT/apps/Nexus-Email"
unset NEXUS_EMAIL_SYSTEM_MAIL_TOKEN
ensure_system_mail_token >/dev/null 2>&1 || fail "a node without Nexus-Email is not an error"
[ -z "${NEXUS_EMAIL_SYSTEM_MAIL_TOKEN:-}" ] || fail "a node without Nexus-Email gets no token"
[ ! -e "$ROOT/apps/Nexus-Email" ] || fail "nothing is created for a node without Nexus-Email"

printf 'PASS: system-mail token is generated once, kept private, and reused\n'
