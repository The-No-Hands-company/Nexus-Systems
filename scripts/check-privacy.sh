#!/usr/bin/env bash
# Fails when code outside the front door reads address headers, or when a
# migration adds an address or user-agent column. The proxy's stripping module
# is the one place allowed to touch these headers.
set -euo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT"
# Historical migrations that CREATED a now-dropped column go here, one per
# line, each with a comment naming the migration that drops the column, e.g.
#   apps/X/migrations/0001_init.sql   # dropped by 0007_zero_retention.sql
# None are needed today: every current migration that creates such a column is
# either already dropped in the same file or not matched by the patterns below.
# (PRIVACY_ALLOW_FILES adds space-separated extras; the test uses it.)
ALLOW_FILES=()
# shellcheck disable=SC2206
ALLOW_FILES+=(${PRIVACY_ALLOW_FILES:-})
HEADERS='cf-connecting-ip|x-forwarded-for|x-real-ip|true-client-ip|cf-ipcountry|x-client-ip'
ALLOW='^(deploy/production/client-tag\.ts|deploy/production/tests/|scripts/check-privacy\.sh|scripts/tests/|docs/)'
fail=0
hits=$(git ls-files 2>/dev/null || find . -type f | sed 's#^\./##')
if [ "${#ALLOW_FILES[@]}" -gt 0 ]; then
  hits=$(printf '%s\n' "$hits" | grep -vxF "$(printf '%s\n' "${ALLOW_FILES[@]}")" || true)
fi
code=$(printf '%s\n' "$hits" | grep -E '\.(ts|tsx|js|mjs|rs|py|go)$' | grep -vE "$ALLOW" | grep -vE '(^|/)(node_modules|target|dist|build|tests?|__tests__)/' | grep -vE '\.(test|spec)\.' || true)
if [ -n "$code" ]; then
  bad=$(printf '%s\n' "$code" | xargs -r grep -liE "[\"'](${HEADERS})[\"']" 2>/dev/null || true)
  [ -n "$bad" ] && { echo "FAIL: address header read outside the front door:"; echo "$bad"; fail=1; }
fi
sql=$(printf '%s\n' "$hits" | grep -E '\.sql$' | grep -vE "$ALLOW" || true)
if [ -n "$sql" ]; then
  badsql=$(printf '%s\n' "$sql" | xargs -r grep -liE 'add column (if not exists )?(ip|ip_address|ip_hash|user_agent|reporter_ip|remote_addr)\b|\b(ip|ip_address|ip_hash|user_agent|reporter_ip)\s+(inet|text|varchar)' 2>/dev/null || true)
  [ -n "$badsql" ] && { echo "FAIL: migration stores an address or user agent:"; echo "$badsql"; fail=1; }
fi
[ "$fail" = 0 ] && echo PASS || exit 1
