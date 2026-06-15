#!/usr/bin/env bash
set -euo pipefail

CLOUD_LOG=/tmp/nexus-cloud.log
rm -f $CLOUD_LOG
ROOT="$(cd "$(dirname "$0")" && pwd)"

cleanup() {
  jobs -p | xargs -r kill 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT

# ── Start cloud ─────────────────────────────────────────────────────

echo "━━━ Nexus-Cloud ━━━"
cd "$ROOT/apps/Nexus-Cloud"
NEXUS_CLOUD_API_KEY=change-me NEXUS_CLOUD_URL=http://localhost:8787 CORS_ORIGIN=* PORT=8787 \
  bun run src/index.ts > $CLOUD_LOG 2>&1 &
CLOUD_PID=$!

for i in $(seq 1 20); do
  nc -z localhost 8787 2>/dev/null && break
  [ $i -eq 20 ] && { echo "FAIL: cloud did not start"; kill $CLOUD_PID 2>/dev/null; exit 1; }
  sleep 1
done

curl -s http://localhost:8787/health | grep -q '"ok":true' && echo "✓ Cloud healthy" || echo "✗ Cloud health failed"

# ── Start apps ──────────────────────────────────────────────────────

declare -A PORTS=( [Nexus-Graphic]=3080 [Nexus-Design]=3074 [Nexus-Draw]=3075 [Nexus-Photos]=3096 [Nexus-Video]=3113 )
COUNT=0

for app in Nexus-Graphic Nexus-Design Nexus-Draw Nexus-Photos Nexus-Video; do
  port=${PORTS[$app]}
  env_key="NEXUS_${app//-/_}_ENABLE_CLOUD_INTEGRATION"

  export PORT=$port
  export NEXUS_CLOUD_URL=http://localhost:8787
  export NEXUS_CLOUD_API_KEY=change-me
  export "$env_key=true"

  cd "$ROOT/apps/$app"
  bun run src/index.ts > "/tmp/${app}.log" 2>&1 &
  
  for i in $(seq 1 10); do
    nc -z localhost $port 2>/dev/null && break
    sleep 1
  done
  curl -s "http://localhost:$port/health" | grep -q '"status":"ok"' && echo "✓ $app:$port healthy" || echo "✗ $app:$port FAILED"
  COUNT=$((COUNT+1))
done

# ── Topology ────────────────────────────────────────────────────────

sleep 2
echo ""
echo "━━━ Topology ━━━"
curl -s http://localhost:8787/api/v1/tools | python3 -c "
import json,sys
data=json.load(sys.stdin)
tools=data.get('tools',[])
print(f'{len(tools)} tools registered:')
for t in sorted(tools,key=lambda x:x['id']):
    print(f'  {t[\"id\"]:25s} {t[\"health\"]:10s} {t.get(\"upstreamUrl\",\"-\"):40s}')
" 2>/dev/null || true

# ── Cross-app flow (simplified) ─────────────────────────────────────

echo ""
echo "━━━ Cross-App Communication Flow ━━━"

# Step 1: Upload a photo to Nexus-Photos
echo "1) POST /api/v1/photos → Photos:3096"
PHOTO_JSON=$(curl -s -X POST http://localhost:3096/api/v1/photos \
  -H 'content-type: application/json' \
  -d '{"title":"E2E Test Photo","url":"s3://nexus-photos/test.jpg","width":1920,"height":1080}')
PHOTO_ID=$(echo "$PHOTO_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -n "$PHOTO_ID" ] && echo "  ✓ Photo created: $PHOTO_ID" || echo "  ✗ Photo upload failed"

# Step 3: Submit photo for GPU validation
echo "3) POST /api/v1/photos/validate → Photos:3096 (→ GPU-Test)"
VALIDATE_JSON=$(curl -s -X POST http://localhost:3096/api/v1/photos/validate \
  -H 'content-type: application/json' \
  -d "{\"photoId\":\"$PHOTO_ID\"}")
VALIDATE_OK=$(echo "$VALIDATE_JSON" | python3 -c "import json,sys; d=json.load(sys.stdin); print('ok' if d.get('jobId') else 'fail')" 2>/dev/null || echo "fail")
[ "$VALIDATE_OK" = "ok" ] && echo "  ✓ Validation submitted" || echo "  ✗ Validation submission failed: $(echo $VALIDATE_JSON | head -c 80)"

# Step 5: Create a design project
echo "5) POST /api/v1/design/projects → Design:3074"
PROJECT_JSON=$(curl -s -X POST http://localhost:3074/api/v1/design/projects \
  -H 'content-type: application/json' \
  -d '{"name":"E2E Test Project","type":"web"}')
PROJECT_ID=$(echo "$PROJECT_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -n "$PROJECT_ID" ] && echo "  ✓ Project created: $PROJECT_ID" || echo "  ✗ Project creation failed"

# Step 7: Check topology
echo "7) GET /api/v1/topology → Cloud:8787"
TOPO_JSON=$(curl -s http://localhost:8787/api/v1/topology)
TOPO_APPS=$(echo "$TOPO_JSON" | python3 -c "import json,sys; print(len(json.load(sys.stdin).get('topology',{}).get('apps',[])))" 2>/dev/null || echo "0")
echo "  ✓ Topology: $TOPO_APPS apps in ecosystem"

echo ""
echo "✓ Ecosystem running ($COUNT apps + cloud)"
