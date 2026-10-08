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
# (PRIVACY_ALLOW_FILES adds space-separated extras; the test uses it.)
ALLOW_FILES=(
  apps/Nexus/crates/nexus-db/migrations/20260218000001_initial_schema.sql        # dropped by 20261007000001_zero_retention.sql
  apps/Nexus/crates/nexus-db/migrations/20260218000009_account_security.sql      # dropped by 20261007000001_zero_retention.sql
  apps/Nexus/crates/nexus-db/migrations/20260404000002_push_subscriptions.sql    # dropped by 20261007000001_zero_retention.sql
  apps/Nexus/crates/nexus-db/migrations/20260405000001_instance_audit_log.sql    # dropped by 20261007000001_zero_retention.sql
  apps/Nexus/crates/nexus-db/migrations-lite/20260218000001_initial.sql          # dropped by migrations-lite/20261007000001_zero_retention.sql
  # Its only address headers are in a unit test proving they are ignored.
  apps/Nexus/crates/nexus-api/src/middleware.rs
)
# shellcheck disable=SC2206
ALLOW_FILES+=(${PRIVACY_ALLOW_FILES:-})
# Directories not started by deploy.sh. Not scanned.
RETIRED_DIRS=(
  # Not started by deploy.sh; retired 2026-10-07 by the founder's decision.
  # Remove from this list if it is ever revived — it still reads req.ip.
  apps/Nexus-API/
  # Not started by deploy.sh. It still reads X-Forwarded-For (rate limits, IP
  # filter, request logs) and must switch to X-Nexus-Client-Tag before it is
  # ever deployed — the check below fails if deploy.sh starts it.
  apps/Nexus-AI/
)
HEADERS='cf-connecting-ip|cf-connecting-ipv6|cf-pseudo-ipv4|x-forwarded-for|x-real-ip|true-client-ip|cf-ipcountry|cf-ray|forwarded|x-client-ip|cf-visitor|cf-ew-via|cdn-loop'
# Direct address reads (case-sensitive).
DIRECT='\b(req|request)\.ip\b|remoteAddress|requestIP\(|ConnectInfo|peer_addr|X_FORWARDED_FOR|XForwardedFor'
# dhts/ecosystem-porter/src/ parses /proc/net/tcp for local port scanning; its
# `remoteAddress` is a socket-table field, not a visitor (dev tool, serves nothing).
ALLOW='^(dhts/ecosystem-porter/src/|deploy/production/client-tag\.ts|deploy/production/tests/|scripts/check-privacy\.sh|scripts/tests/|docs/)'
Q="[\"'\`]"
fail=0
# The services TNHC runs live in submodules (Chat, Hosting, Cloud, ...). An
# unfetched submodule scans as zero files and would pass hollow, so every one
# except the dev-only game toolset must be checked out.
while read -r _ sm _; do
  [ "$sm" = dhts/GameDevelopmentToolset ] && continue
  [ -n "$(ls -A "$sm" 2>/dev/null)" ] || { echo "FAIL: submodule $sm is not checked out, so it cannot be scanned"; fail=1; }
done < <(git submodule status 2>/dev/null | sed 's/^[-+U ]//')
for d in "${RETIRED_DIRS[@]}"; do
  if grep -qF "${d%/}" deploy/production/deploy.sh 2>/dev/null; then
    echo "FAIL: $d is excluded from this check but deploy.sh starts it"; fail=1
  fi
done
hits=$(git ls-files --recurse-submodules 2>/dev/null || find . -type f | sed 's#^\./##')
if [ "${#ALLOW_FILES[@]}" -gt 0 ]; then
  hits=$(printf '%s\n' "$hits" | grep -vxF "$(printf '%s\n' "${ALLOW_FILES[@]}")" || true)
fi
for d in "${RETIRED_DIRS[@]}"; do
  hits=$(printf '%s\n' "$hits" | awk -v d="$d" 'index($0,d)!=1' || true)
done
code=$(printf '%s\n' "$hits" | grep -E '\.(ts|tsx|js|mjs|cjs|jsx|mts|cts|rs|py|go|svelte|vue|sh)$' | grep -vE "$ALLOW" | grep -vE '(^|/)(node_modules|target|dist)/' | grep -vE '(^|/)(tests|__tests__)/' | grep -vE '\.(test|spec)\.' || true)
if [ -n "$code" ]; then
  bad=$( { printf '%s\n' "$code" | xargs -r grep -liE "${Q}(${HEADERS})${Q}" 2>/dev/null;
           printf '%s\n' "$code" | xargs -r grep -lE "$DIRECT" 2>/dev/null; } | sort -u || true)
  [ -n "$bad" ] && { echo "FAIL: address read outside the front door:"; echo "$bad"; fail=1; }
fi
sql=$(printf '%s\n' "$hits" | grep -E '\.sql$' | grep -vE "$ALLOW" || true)
if [ -n "$sql" ]; then
  badsql=$(printf '%s\n' "$sql" | xargs -r grep -liE 'add column (if not exists )?(ip|ip_address|ip_hash|user_agent|reporter_ip|remote_addr)\b|\b(ip|ip_address|ip_hash|user_agent|reporter_ip)\s+(inet|text|varchar)' 2>/dev/null || true)
  [ -n "$badsql" ] && { echo "FAIL: migration stores an address or user agent:"; echo "$badsql"; fail=1; }
fi
[ "$fail" = 0 ] && echo PASS || exit 1
