#!/usr/bin/env bash
# Nexus Systems Smoke Test — start Cloud + all apps, verify health
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APPS="$ROOT/apps"
RESULTS="/tmp/nexus-smoke-results.txt"
> "$RESULTS"

CLOUD_LOG="/tmp/nexus-cloud-smoke.log"
rm -f "$CLOUD_LOG"

cleanup() {
  jobs -p | xargs -r kill 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT

# ── Start Nexus-Cloud ──────────────────────────────────────────────

echo "━━━ Starting Nexus-Cloud on :8787 ━━━"
cd "$APPS/Nexus-Cloud"
NEXUS_CLOUD_API_KEY=change-me CORS_ORIGIN=* PORT=8787 \
  bun run src/index.ts > "$CLOUD_LOG" 2>&1 &
CLOUD_PID=$!

for i in $(seq 1 25); do
  if nc -z localhost 8787 2>/dev/null; then
    curl -s http://localhost:8787/health | grep -q '"ok":true' && echo "✓ Cloud ready" || echo "✗ Cloud health failed"
    break
  fi
  [ $i -eq 25 ] && { echo "✗ Cloud failed to start"; exit 1; }
  sleep 1
done

# ── Scan and start all apps ────────────────────────────────────────

declare -A APP_PORTS
APP_PIDS=()

echo ""
echo "━━━ Starting apps ━━━"

for app_dir in "$APPS"/*/; do
  name=$(basename "$app_dir")
  
  # Skip non-apps
  [[ "$name" == "Nexus-Cloud" ]] && continue
  [[ "$name" == "Phantom" ]] && continue
  [[ "$name" == "Nexus-Modeling" ]] && continue
  [[ "$name" == Nexuslang ]] && continue
  [[ "$name" == docs ]] && continue
  [[ "$name" == data ]] && continue
  [[ "$name" == tests ]] && continue
  [[ "$name" == .* ]] && continue
  
  # Must have package.json
  [[ ! -f "$app_dir/package.json" ]] && continue
  
  # Get port from server.ts
  port=$(grep -oP 'PORT\s*\|\|\s*"\K\d+' "$app_dir/src/server.ts" 2>/dev/null || true)
  [[ -z "$port" ]] && continue
  
  # Start the app
  env_key="NEXUS_${name//-/_}_ENABLE_CLOUD_INTEGRATION"
  export PORT="$port"
  export "$env_key=true"
  export NEXUS_CLOUD_URL="http://localhost:8787"
  export NEXUS_CLOUD_API_KEY="change-me"
  
  cd "$app_dir"
  bun run src/index.ts > "/tmp/${name}-smoke.log" 2>&1 &
  APP_PIDS+=($!)
  APP_PORTS["$name"]="$port"
  
  echo -n "."
done

echo ""
echo "Started ${#APP_PIDS[@]} apps"

# ── Wait and verify ────────────────────────────────────────────────

echo ""
echo "━━━ Health checks ━━━"
sleep 3

TOTAL=${#APP_PORTS[@]}
PASS=0
FAIL=0

for name in "${!APP_PORTS[@]}"; do
  port="${APP_PORTS[$name]}"
  
  if nc -z localhost "$port" 2>/dev/null; then
    health_ok=false
    for retry in $(seq 1 5); do
      if curl -s -m 2 "http://localhost:$port/health" | -E '("status":"ok"|"ok":true)'; then
        health_ok=true; break
      fi
      sleep 1
    done
    if $health_ok; then
      echo "  ✓ $name :$port"
      ((PASS++)) || true
      echo "$name:$port:PASS" >> "$RESULTS"
    else
      echo "  ✗ $name :$port (health failed)"
      ((FAIL++)) || true
      echo "$name:$port:FAIL_HEALTH" >> "$RESULTS"
    fi
  else
    echo "  ✗ $name :$port (not listening)"
    ((FAIL++)) || true
    echo "$name:$port:FAIL_PORT" >> "$RESULTS"
  fi
done

echo ""
echo "━━━ Results ━━━"
echo "  Total: $TOTAL"
echo "  Pass:  $PASS"
echo "  Fail:  $FAIL"

exit $FAIL
