#!/usr/bin/env bash
# Sends requests through the front door (127.0.0.1:8080) carrying a unique
# documentation-range address, user agent and query string, then searches every
# place data could land. Exit 1 names each location where a marker was found.
# Nothing here writes the marker to persistent storage: dumps and logs are
# piped straight into grep, and the result file lives on tmpfs.
set -uo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
LOG_DIR="${LOG_DIR:-/tmp/nexus-production}"
OUT="${CANARY_OUT:-/tmp/nexus-production/privacy-canary.json}"
PROXY="${CANARY_PROXY:-http://127.0.0.1:8080}"
ADDR="203.0.113.$(( (RANDOM % 250) + 2 ))"
MARK="canary-$(date +%s)-$RANDOM-$RANDOM"

hits=()
add_hit() { hits+=("$1"); }

# search_logs: any *.log / *.log.1 in $LOG_DIR containing a marker.
search_logs() {
  local f
  for f in "$LOG_DIR"/*.log "$LOG_DIR"/*.log.1; do
    [ -f "$f" ] || continue
    grep -qF -e "$ADDR" -e "$MARK" "$f" 2>/dev/null && add_hit "log:$f"
  done
}

if [ "${1:-}" = "--self-test" ]; then
  d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
  LOG_DIR="$d"
  search_logs
  [ ${#hits[@]} -eq 0 ] || { echo "SELF-TEST FAIL: clean dir reported hits"; exit 1; }
  printf 'GET /?%s from %s\n' "$MARK" "$ADDR" > "$d/planted.log"
  search_logs
  [ ${#hits[@]} -eq 1 ] && [ "${hits[0]}" = "log:$d/planted.log" ] \
    || { echo "SELF-TEST FAIL: planted marker not found (hits=${hits[*]:-none})"; exit 1; }
  echo "SELF-TEST PASS"
  exit 0
fi

for host in auth.tnhc.dev app.tnhc.dev cloud.tnhc.dev chat.tnhc.dev hosting.tnhc.dev storage.tnhc.dev draw.tnhc.dev email-ingress.tnhc.dev; do
  curl -s -o /dev/null -m 10 -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "X-Forwarded-For: $ADDR" \
    -H "User-Agent: $MARK" "$PROXY/?$MARK" || true
  curl -s -o /dev/null -m 10 -X POST -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "X-Forwarded-For: $ADDR" \
    -H "User-Agent: $MARK" -H 'Content-Type: application/json' \
    --data '{"username":"canary","password":"canary"}' "$PROXY/api/v1/auth/login" || true # pragma: allowlist secret
done
# email-ingress without a token: expected 401, must still store nothing.
curl -s -o /dev/null -m 10 -X POST -H "Host: email-ingress.tnhc.dev" -H "CF-Connecting-IP: $ADDR" \
  -H "User-Agent: $MARK" -H 'Content-Type: application/json' --data '{}' \
  "$PROXY/internal/v1/cloudflare-email?$MARK" || true
sleep 5

# 1. native logs
search_logs

# 2. every running container's recent logs
for c in $(docker ps --format '{{.Names}}'); do
  docker logs --since 10m "$c" 2>&1 | grep -qF -e "$ADDR" -e "$MARK" && add_hit "container:$c"
done

# 3. every production database (data-only dump piped straight into grep)
for spec in \
  "nexus-systems-postgres-1 nexus nexus" "nexus-systems-postgres-1 nexus nexus_chat" \
  "nexus-systems-postgres-1 nexus nexus_cloud" "nexus-systems-postgres-1 nexus nexus_email" \
  "nexus-hosting-db-1 nexus nexus" "supabase-db postgres postgres"; do
  read -r ctr usr db <<<"$spec"
  r=$(docker exec "$ctr" pg_dump -U "$usr" -d "$db" --data-only 2>/dev/null | grep -cF -e "$ADDR" -e "$MARK"; echo "rc=${PIPESTATUS[0]}")
  # A dump that failed cannot prove the database clean, so it fails the run.
  case "$r" in *"rc=0") ;; *) add_hit "error:db-dump-failed:$ctr/$db"; continue ;; esac
  [ "$(head -n1 <<<"$r")" != "0" ] && add_hit "db:$ctr/$db"
done

# 4. Auth's JSON stores
for f in "$ROOT"/apps/Nexus-Auth/data/*.json; do
  [ -f "$f" ] || continue
  grep -qF -e "$ADDR" -e "$MARK" "$f" 2>/dev/null && add_hit "file:$f"
done

# 5. Terminal's audit SQLite (dumped into grep, never to disk)
for f in "$ROOT"/data/terminal-audit.sqlite "$ROOT"/apps/Nexus-Terminal/data/terminal-audit.sqlite; do
  [ -f "$f" ] || continue
  sqlite3 "file:$f?mode=ro" .dump 2>/dev/null | grep -qF -e "$ADDR" -e "$MARK" && add_hit "sqlite:$f"
done

status=$([ ${#hits[@]} -eq 0 ] && echo pass || echo fail)
mkdir -p "$(dirname "$OUT")"
printf '{"at":"%s","address":"%s","status":"%s","found":%s}\n' "$(date -u +%FT%TZ)" "$ADDR" "$status" \
  "$(printf '%s\n' "${hits[@]:-}" | jq -R . | jq -s 'map(select(length>0))')" > "$OUT"
cat "$OUT"
[ "$status" = pass ]
