#!/usr/bin/env bash
# Sends requests through the front door (127.0.0.1:8080) carrying a unique
# documentation-range address, user agent and query string, then searches every
# place data could land. Exit 1 names each location where a marker was found,
# and also fails when a source could not be checked (a hollow pass is a fail).
# Nothing here writes the marker to persistent storage: dumps and logs are
# piped straight into grep, the result file lives on tmpfs, and stdout/stderr
# carry only status, counts and locations, never the address or token.
set -uo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
LOG_DIR="${LOG_DIR:-/tmp/nexus-production}"
OUT="${CANARY_OUT:-/tmp/nexus-production/privacy-canary.json}"
PROXY="${CANARY_PROXY:-http://127.0.0.1:8080}"
SETTLE="${CANARY_SETTLE_SECONDS:-70}"
ADDR="203.0.113.$(( (RANDOM % 250) + 2 ))"
MARK="canary-$(date +%s)-$RANDOM-$RANDOM"

hits=()
add_hit() { hits+=("$1"); }
n_logs=0; n_containers=0; n_dbs=0; n_files=0; answered=0

# scan_cmd <cmd...>: runs the producer into a counting grep (never grep -q, so
# the producer is read to the end and cannot die of SIGPIPE). Sets SCAN_N (match
# lines), SCAN_PRC (producer status) and SCAN_GRC (grep status; 2 = grep error).
scan_cmd() {
  local out
  out=$("$@" 2>/dev/null | grep -cF -e "$ADDR" -e "$MARK"; echo "${PIPESTATUS[0]} ${PIPESTATUS[1]}")
  SCAN_N=$(head -n1 <<<"$out")
  read -r SCAN_PRC SCAN_GRC <<<"$(tail -n1 <<<"$out")"
  case "$SCAN_N" in ''|*[!0-9]*) SCAN_N=0; SCAN_GRC=2 ;; esac
}
# judge <found-label> <error-label>: turn the last scan into at most one hit.
judge() {
  if [ "$SCAN_PRC" != 0 ] || [ "${SCAN_GRC:-2}" -gt 1 ]; then add_hit "error:$2"
  elif [ "$SCAN_N" -gt 0 ]; then add_hit "$1"; fi
}

search_logs() {
  local f
  for f in "$LOG_DIR"/*.log "$LOG_DIR"/*.log.1; do
    [ -f "$f" ] || continue
    n_logs=$((n_logs + 1))
    scan_cmd cat "$f"; judge "log:$f" "log-unreadable:$f"
  done
}

if [ "${1:-}" = "--self-test" ]; then
  me="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
  d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
  fail() { echo "SELF-TEST FAIL: $1"; exit 1; }
  LOG_DIR="$d"
  search_logs
  [ ${#hits[@]} -eq 0 ] || fail "clean dir reported hits"
  printf 'GET /?%s from %s\n' "$MARK" "$ADDR" > "$d/planted.log"
  search_logs
  [ ${#hits[@]} -eq 1 ] && [ "${hits[0]}" = "log:$d/planted.log" ] \
    || fail "planted marker not found (hits=${hits[*]:-none})"
  # marker at the start of a >1 MB stream must still be found (SIGPIPE case)
  scan_cmd bash -c 'printf "%s\n" "$0"; head -c 3000000 /dev/zero | tr "\0" x' "$MARK"
  { [ "$SCAN_N" = 1 ] && [ "$SCAN_PRC" = 0 ]; } || fail "marker in large stream lost (n=$SCAN_N prc=$SCAN_PRC)"
  # a failing producer must be an error, not "not found"
  scan_cmd cat "$d/does-not-exist"; hits=(); judge x "unreadable"
  [ "${hits[0]:-}" = "error:unreadable" ] || fail "unreadable source not an error"
  # dead proxy: the whole canary must fail with error:proxy-unreachable
  CANARY_PROXY="http://127.0.0.1:1" CANARY_SETTLE_SECONDS=0 CANARY_OUT="$d/out.json" LOG_DIR="$d" bash "$me" >/dev/null 2>&1
  rc=$?
  [ "$rc" = 1 ] || fail "dead proxy did not exit 1 (rc=$rc)"
  jq -e '.status=="fail" and (.found|index("error:proxy-unreachable"))' "$d/out.json" >/dev/null \
    || fail "dead proxy not reported as error:proxy-unreachable"
  echo "SELF-TEST PASS"
  exit 0
fi

probe() { # probe <curl args...>: counts any HTTP response
  local code
  code=$(curl -s -o /dev/null -m 10 -w '%{http_code}' "$@" 2>/dev/null) || code=000
  [ "$code" != 000 ] && [ -n "$code" ] && answered=$((answered + 1))
  return 0
}
for host in auth.tnhc.dev app.tnhc.dev cloud.tnhc.dev chat.tnhc.dev hosting.tnhc.dev storage.tnhc.dev draw.tnhc.dev email-ingress.tnhc.dev demo.tnhc.dev calendar.tnhc.dev; do
  probe -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "X-Forwarded-For: $ADDR" \
    -H "User-Agent: $MARK" "$PROXY/?$MARK"
  probe -X POST -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "X-Forwarded-For: $ADDR" \
    -H "User-Agent: $MARK" -H 'Content-Type: application/json' \
    --data '{"username":"canary","password":"canary"}' "$PROXY/api/v1/auth/login" # pragma: allowlist secret
done
# email-ingress without a token: expected 401, must still store nothing.
probe -X POST -H "Host: email-ingress.tnhc.dev" -H "CF-Connecting-IP: $ADDR" \
  -H "User-Agent: $MARK" -H 'Content-Type: application/json' --data '{}' \
  "$PROXY/internal/v1/cloudflare-email?$MARK"
[ "$answered" -gt 0 ] || add_hit "error:proxy-unreachable"

# Let async writers drain (Auth's 5 s buffer, Hosting's 60 s rollup).
sleep "$SETTLE"

# 1. native logs
search_logs

# 2. every running container's recent logs
containers=$(docker ps --format '{{.Names}}' 2>/dev/null) || containers=""
if [ -z "$containers" ]; then add_hit "error:docker-unavailable"; else
  for c in $containers; do
    n_containers=$((n_containers + 1))
    # Log driver "none" stores nothing by design: there is nothing to read.
    [ "$(docker inspect -f '{{.HostConfig.LogConfig.Type}}' "$c" 2>/dev/null)" = none ] && continue
    scan_cmd bash -c 'docker logs --since 10m "$0" 2>&1' "$c"; judge "container:$c" "container-log-failed:$c"
  done
fi

# 3. every production database (data-only dump piped straight into grep)
for spec in \
  "nexus-systems-postgres-1 nexus nexus" "nexus-systems-postgres-1 nexus nexus_chat" \
  "nexus-systems-postgres-1 nexus nexus_cloud" "nexus-systems-postgres-1 nexus nexus_email" \
  "nexus-hosting-db-1 nexus nexus" "supabase-db postgres postgres"; do
  read -r ctr usr db <<<"$spec"
  n_dbs=$((n_dbs + 1))
  scan_cmd docker exec "$ctr" pg_dump -U "$usr" -d "$db" --data-only
  judge "db:$ctr/$db" "db-dump-failed:$ctr/$db"
done

# 4. Auth's JSON stores
shopt -s nullglob
auth_files=("$ROOT"/apps/Nexus-Auth/data/*.json)
[ ${#auth_files[@]} -gt 0 ] || add_hit "error:auth-files-missing"
for f in "${auth_files[@]}"; do
  n_files=$((n_files + 1))
  scan_cmd cat "$f"; judge "file:$f" "file-unreadable:$f"
done

# 5. Terminal's audit SQLite (dumped into grep, never to disk)
f="$ROOT/data/terminal-audit.sqlite"
if [ -f "$f" ]; then
  n_files=$((n_files + 1))
  if ! command -v sqlite3 >/dev/null 2>&1; then add_hit "error:sqlite-dump-failed:$f"
  else scan_cmd sqlite3 "file:$f?mode=ro" .dump; judge "sqlite:$f" "sqlite-dump-failed:$f"; fi
fi

status=$([ ${#hits[@]} -eq 0 ] && echo pass || echo fail)
mkdir -p "$(dirname "$OUT")"
printf '{"at":"%s","address":"%s","status":"%s","found":%s,"sources":{"logs":%d,"containers":%d,"databases":%d,"files":%d,"probes_answered":%d}}\n' \
  "$(date -u +%FT%TZ)" "$ADDR" "$status" \
  "$(printf '%s\n' "${hits[@]:-}" | jq -R . | jq -s 'map(select(length>0))')" \
  "$n_logs" "$n_containers" "$n_dbs" "$n_files" "$answered" > "$OUT"
# stdout: status, counts and locations only - never the address or token.
echo "privacy-canary: $status logs=$n_logs containers=$n_containers databases=$n_dbs files=$n_files probes_answered=$answered"
[ ${#hits[@]} -gt 0 ] && printf 'found: %s\n' "${hits[@]}"
[ "$status" = pass ]
