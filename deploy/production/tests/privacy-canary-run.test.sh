#!/usr/bin/env bash
# After a deploy the privacy check runs once, so status.tnhc.dev reflects the
# state the deploy produced instead of the last daily run (which may have
# fallen inside an outage and still read "fail"). Never blocks the deploy.
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCTION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nexus-canary-run-test.XXXXXX")"
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

# A stand-in systemctl records its arguments; the real one is never called.
mkdir -p "$TEST_ROOT/bin"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" "$*" >> "%s/systemctl.args"\n' "$TEST_ROOT" > "$TEST_ROOT/bin/systemctl"
chmod +x "$TEST_ROOT/bin/systemctl"
PATH="$TEST_ROOT/bin:$PATH"

NEXUS_SKIP_CANARY_INSTALL=1 run_privacy_canary_now
[ ! -e "$TEST_ROOT/systemctl.args" ] || fail "a skipped canary is not run"

NEXUS_SKIP_CANARY_INSTALL=0 run_privacy_canary_now
grep -qx -- "--user start --no-block nexus-privacy-canary.service" "$TEST_ROOT/systemctl.args" \
    || fail "the canary is started once, without waiting for it"

body="$(sed -n '/^cmd_start()/,/^}/p' "$PRODUCTION_DIR/deploy.sh")"
status_line="$(grep -n '^    cmd_status$' <<<"$body" | cut -d: -f1)"
run_line="$(grep -n '^    run_privacy_canary_now$' <<<"$body" | cut -d: -f1)"
[ -n "$status_line" ] && [ -n "$run_line" ] && [ "$run_line" -gt "$status_line" ] \
    || fail "cmd_start runs the canary after the services are verified"

printf 'PASS: the privacy check runs once after every deploy, without blocking it\n'
