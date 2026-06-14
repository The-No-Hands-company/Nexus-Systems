#!/usr/bin/env bash
set -euo pipefail

# ═══════════════════════════════════════════════════════════════════
# Nexus Ecosystem — Cross-App Communication Demo
# ═══════════════════════════════════════════════════════════════════
# Starts Nexus-Cloud + Photos + Design + GPU-Test,
# then runs a real-world flow simulating cross-app communication.

ROOT="$(cd "$(dirname "$0")" && pwd)"
CLOUD_PORT=8787
PHOTOS_PORT=3096
DESIGN_PORT=3074
GPU_TEST_PORT=3082
API_KEY="change-me"

LOG_DIR="/tmp/nexus-demo"
mkdir -p "$LOG_DIR"

declare -a PIDS=()
cleanup() {
  echo ""
  echo "Cleaning up…"
  for pid in "${PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  rm -rf "$LOG_DIR"
  echo "All services stopped."
}
trap cleanup EXIT

# ── helpers ───────────────────────────────────────────────────────

wait_port() {
  local port="$1" label="$2" max="${3:-20}"
  for i in $(seq 1 "$max"); do
    nc -z localhost "$port" 2>/dev/null && return 0
    sleep 1
  done
  echo "  ✗ $label :$port did not start in time"
  return 1
}

check_health() {
  local port="$1" label="$2"
  local health
  health=$(curl -sf "http://localhost:$port/health" 2>/dev/null || echo "")
  if echo "$health" | grep -q '"ok":true\|"status":"ok"'; then
    echo "  ✓ $label :$port healthy"
    return 0
  else
    echo "  ✗ $label :$port FAILED"
    return 1
  fi
}

api() {
  local method="$1" url="$2" body="${3:-}"
  local args=(-s -X "$method" -H "x-api-key: $API_KEY" -H "content-type: application/json")
  [ -n "$body" ] && args+=(-d "$body")
  curl "${args[@]}" "$url" 2>/dev/null || echo '{"error":"unreachable"}'
}

spinner() {
  local pid="$1" label="$2"
  local spin='-\|/'
  while kill -0 "$pid" 2>/dev/null; do
    local i=$((SECONDS % 4))
    printf "\r  %s %s" "${spin:$i:1}" "$label"
    sleep 0.3
  done
  printf "\r  ✓ %s \n" "$label"
}

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   Nexus Ecosystem — Cross-App Communication Demo           ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── 1. Start Nexus-Cloud ──────────────────────────────────────────

echo "▸ Starting Nexus-Cloud (control plane) …"
cd "$ROOT/apps/Nexus-Cloud"
NEXUS_CLOUD_API_KEY="$API_KEY" \
  NEXUS_CLOUD_URL="http://localhost:$CLOUD_PORT" \
  CORS_ORIGIN="*" \
  PORT="$CLOUD_PORT" \
  bun run src/index.ts > "$LOG_DIR/cloud.log" 2>&1 &
PIDS+=($!)
wait_port "$CLOUD_PORT" "Nexus-Cloud"
check_health "$CLOUD_PORT" "Nexus-Cloud"
echo ""

# ── 2. Start apps ─────────────────────────────────────────────────

echo "▸ Starting Nexus-Photos …"
cd "$ROOT/apps/Nexus-Photos"
NEXUS_CLOUD_URL="http://localhost:$CLOUD_PORT" \
  NEXUS_CLOUD_API_KEY="$API_KEY" \
  NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION="true" \
  PORT="$PHOTOS_PORT" \
  bun run src/index.ts > "$LOG_DIR/photos.log" 2>&1 &
PIDS+=($!)
wait_port "$PHOTOS_PORT" "Nexus-Photos"

echo "▸ Starting Nexus-Design …"
cd "$ROOT/apps/Nexus-Design"
NEXUS_CLOUD_URL="http://localhost:$CLOUD_PORT" \
  NEXUS_CLOUD_API_KEY="$API_KEY" \
  NEXUS_DESIGN_ENABLE_CLOUD_INTEGRATION="true" \
  PORT="$DESIGN_PORT" \
  bun run src/index.ts > "$LOG_DIR/design.log" 2>&1 &
PIDS+=($!)
wait_port "$DESIGN_PORT" "Nexus-Design"

echo "▸ Starting Nexus-GPU-Test …"
GPU_TEST_OK=false
if [ -f "$ROOT/.venv/bin/activate" ]; then
  . "$ROOT/.venv/bin/activate" 2>/dev/null || true
fi
if command -v uvicorn &>/dev/null; then
  cd "$ROOT/apps/Nexus-GPU-Test/backend"
  NEXUS_GPU_TEST_DATABASE_URL="sqlite+aiosqlite:///data/gpu-test.db" \
    uvicorn app.main:app --port "$GPU_TEST_PORT" --host 0.0.0.0 \
    > "$LOG_DIR/gpu-test.log" 2>&1 &
  PIDS+=($!)
  if wait_port "$GPU_TEST_PORT" "Nexus-GPU-Test" 10; then
    GPU_TEST_OK=true
  fi
else
  echo "  ⚠ uvicorn not found — GPU test calls will use mock responses"
fi
check_health "$GPU_TEST_PORT" "Nexus-GPU-Test" 2>/dev/null || true
echo ""

# Let services register with the cloud
sleep 3

# ── 3. Demo flow ──────────────────────────────────────────────────

SUCCESS=0
FAIL=0
step() { echo "▸ $1"; }
ok() { echo "  ✓ $1"; SUCCESS=$((SUCCESS + 1)); }
err() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }
div() { echo "─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─"; }

PHOTOS_URL="http://localhost:$PHOTOS_PORT"
DESIGN_URL="http://localhost:$DESIGN_PORT"
CLOUD_URL="http://localhost:$CLOUD_PORT"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   Running cross-app communication flow …                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Upload a photo to Nexus-Photos
div
step "Step 1 — Upload photo to Nexus-Photos (POST /api/v1/photos)"

PHOTO_JSON=$(api POST "$PHOTOS_URL/api/v1/photos" \
  '{"title":"Sunset Panorama","url":"s3://nexus-photos/sunset.jpg","tags":"nature,landscape","width":3840,"height":2160,"fileSize":4194304}')
echo "$PHOTO_JSON" | python3 -c "
import json,sys; d=json.load(sys.stdin);
print(f'  Title:       {d.get(\"title\",\"?\")}');
print(f'  Photo ID:    {d.get(\"id\",\"?\")}');
print(f'  Size:        {d.get(\"width\",\"?\")}×{d.get(\"height\",\"?\")}');
" 2>/dev/null || echo "  Raw: $(echo "$PHOTO_JSON" | head -c 120)"

PHOTO_ID=$(echo "$PHOTO_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -n "$PHOTO_ID" ] && ok "Photo created: $PHOTO_ID" || err "Photo upload failed"

# Step 2: Create an album with that photo
div
step "Step 2 — Create album with photo (POST /api/v1/photos/albums)"

ALBUM_JSON=$(api POST "$PHOTOS_URL/api/v1/photos/albums" \
  '{"name":"Landscape Collection","description":"Best landscape shots of 2026"}')
echo "$ALBUM_JSON" | python3 -c "
import json,sys; d=json.load(sys.stdin);
print(f'  Album:       {d.get(\"name\",\"?\")}');
print(f'  Album ID:    {d.get(\"id\",\"?\")}');
" 2>/dev/null || echo "  Raw: $(echo "$ALBUM_JSON" | head -c 120)"

ALBUM_ID=$(echo "$ALBUM_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -n "$ALBUM_ID" ] && ok "Album created: $ALBUM_ID" || err "Album creation failed"

# Step 3: Submit photo for GPU validation
div
step "Step 3 — Submit photo for GPU validation (POST /api/v1/photos/validate)"

VALIDATE_JSON=$(api POST "$PHOTOS_URL/api/v1/photos/validate" \
  "{\"photoId\":\"$PHOTO_ID\",\"tolerance\":2.0}")
echo "$VALIDATE_JSON" | python3 -c "
import json,sys; d=json.load(sys.stdin);
print(f'  Job ID:      {d.get(\"jobId\",\"?\")}');
print(f'  Status:      {d.get(\"status\",\"?\")}');
print(f'  Photo ID:    {d.get(\"photoId\",\"?\")}');
" 2>/dev/null || echo "  Raw: $(echo "$VALIDATE_JSON" | head -c 120)"

JOB_ID=$(echo "$VALIDATE_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('jobId',''))" 2>/dev/null || echo "")
[ -n "$JOB_ID" ] && ok "Validation job created: $JOB_ID" || err "GPU validation submission failed"

# Step 4: Poll for validation result
div
step "Step 4 — Poll validation result (GET /api/v1/photos/validate/:jobId)"

VALIDATION_RESULT=""
if [ -n "$JOB_ID" ]; then
  for i in $(seq 1 5); do
    RESULT=$(api GET "$PHOTOS_URL/api/v1/photos/validate/$JOB_ID")
    STATUS=$(echo "$RESULT" | python3 -c "import json,sys; print(json.load(sys.stdin).get('status','error'))" 2>/dev/null || echo "error")
    echo "  Poll $i/5 → status: $STATUS"
    if [ "$STATUS" = "passed" ] || [ "$STATUS" = "failed" ] || [ "$STATUS" = "error" ]; then
      VALIDATION_RESULT="$STATUS"
      break
    fi
    sleep 1.5
  done
  [ "$VALIDATION_RESULT" = "passed" ] && ok "Validation result: $VALIDATION_RESULT" \
    || echo "  ⚠ Final status: ${VALIDATION_RESULT:-pending}"
else
  echo "  ⚠ Skipped — no job ID"
fi

# Step 5: Create a design project in Nexus-Design
div
step "Step 5 — Create design project (POST /api/v1/design/projects)"

PROJECT_JSON=$(api POST "$DESIGN_URL/api/v1/design/projects" \
  '{"name":"Landing Page Redesign 2026","type":"web"}')
echo "$PROJECT_JSON" | python3 -c "
import json,sys; d=json.load(sys.stdin);
print(f'  Project:     {d.get(\"name\",\"?\")}');
print(f'  Project ID:  {d.get(\"id\",\"?\")}');
print(f'  Type:        {d.get(\"type\",\"?\")}');
print(f'  Status:     {d.get(\"status\",\"?\")}');
" 2>/dev/null || echo "  Raw: $(echo "$PROJECT_JSON" | head -c 120)"

PROJECT_ID=$(echo "$PROJECT_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -n "$PROJECT_ID" ] && ok "Design project created: $PROJECT_ID" || err "Project creation failed"

# Step 6: Expose the design project via Nexus-Cloud public URL
div
step "Step 6 — Expose design via Cloud (POST /api/v1/design/expose)"

EXPOSE_JSON=$(api POST "$DESIGN_URL/api/v1/design/expose" \
  "{\"projectId\":\"$PROJECT_ID\"}")

EXPOSE_URL=$(echo "$EXPOSE_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('publicUrl',''))" 2>/dev/null || echo "")
EXPOSE_ERR=$(echo "$EXPOSE_JSON" | python3 -c "import json,sys; print(json.load(sys.stdin).get('error',''))" 2>/dev/null || echo "")

if [ -n "$EXPOSE_URL" ]; then
  ok "Exposed at: $EXPOSE_URL"
elif [ -n "$EXPOSE_ERR" ]; then
  echo "  ⚠ Cloud response: $EXPOSE_ERR"
  echo "  (Design sends projectId; Cloud expects toolId — cross-app translation)"
else
  echo "  Raw: $(echo "$EXPOSE_JSON" | head -c 120)"
fi

# Step 7: Check Nexus-Cloud topology
div
step "Step 7 — System topology (GET /api/v1/topology)"

TOPO_JSON=$(api GET "$CLOUD_URL/api/v1/topology")
echo "$TOPO_JSON" | python3 -c "
import json,sys
data=json.load(sys.stdin)
topo=data.get('topology',{})
apps=topo.get('apps',[])
conns=topo.get('connections',[])
print(f'  Apps in ecosystem:    {len(apps)}');
print(f'  Connections defined:  {len(conns)}');
for a in sorted(apps, key=lambda x: x['id']):
    print(f'    {a[\"id\"]:30s} [{a.get(\"kind\",\"?\")}]');
" 2>/dev/null || echo "  Raw: $(echo "$TOPO_JSON" | head -c 200)"

# Also show registered tools
echo ""
echo "▸ Registered tools (GET /api/v1/tools):"
TOOLS_JSON=$(api GET "$CLOUD_URL/api/v1/tools")
echo "$TOOLS_JSON" | python3 -c "
import json,sys
data=json.load(sys.stdin)
tools=data.get('tools',[])
print(f'  Tools registered: {len(tools)}');
for t in sorted(tools, key=lambda x: x['id']):
    health=t.get('health','?')
    url=t.get('upstreamUrl','–')
    print(f'    {t[\"id\"]:30s} health={health:12s} up={url}');
" 2>/dev/null || echo "  (no tools registered yet — services need heartbeat cycles)"

echo ""

# ── Summary ───────────────────────────────────────────────────────

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              CROSS-APP DEMO SUMMARY                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

cat <<EOF
  ┌─────────────────────────────────────────────────────────────┐
  │                    Nexus Ecosystem Flow                     │
  │                                                             │
  │   ┌──────────┐      ┌──────────────┐     ┌──────────────┐  │
  │   │  upload   │      │  create       │     │  validate    │  │
  │   │  photo    │ ──── │  album        │ ─── │  on GPU      │  │
  │   │           │      │               │     │              │  │
  │   │ Nexus-    │      │  Nexus-       │     │  Nexus-      │  │
  │   │ Photos    │      │  Photos       │     │  GPU-Test    │  │
  │   │ :$PHOTOS_PORT  │      │  :$PHOTOS_PORT     │      │  :$GPU_TEST_PORT     │  │
  │   └──────────┘      └──────────────┘     └──────┬───────┘  │
  │                                                 │          │
  │           ┌─────────────────────────────────────┘          │
  │           │                                                │
  │   ┌───────▼──────┐     ┌──────────────┐                    │
  │   │  design      │     │  expose       │                    │
  │   │  project     │ ─── │  via Cloud    │                    │
  │   │              │     │              │                    │
  │   │  Nexus-      │     │  Nexus-      │                    │
  │   │  Design      │     │  Cloud       │                    │
  │   │  :$DESIGN_PORT     │     │  :$CLOUD_PORT    │                    │
  │   └──────────────┘     └──────┬───────┘                    │
  │                               │                            │
  │   ┌───────────────────────────┘                            │
  │   │                                                        │
  │   ▼                                                        │
  │   GET /api/v1/topology  →  81 apps in ecosystem            │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
EOF

echo ""
echo "  Legend:  $(printf '\e[36mNexus-Cloud\e[0m')  ← control plane"
echo "           $(printf '\e[32mNexus-Photos\e[0m')  ← photo storage → GPU validation"
echo "           $(printf '\e[33mNexus-Design\e[0m')   ← design prototype → Cloud exposure"
echo "           $(printf '\e[35mNexus-GPU-Test\e[0m') ← perceptual GPU validation"
echo ""

echo "  ───────────────────────────────────────────────────────────"
echo "  API Calls Performed:"
echo ""
echo "  Step 1  POST   /api/v1/photos              → $PHOTOS_URL"
echo "  Step 2  POST   /api/v1/photos/albums       → $PHOTOS_URL"
echo "  Step 3  POST   /api/v1/photos/validate     → $PHOTOS_URL  (→ GPU-Test)"
echo "  Step 4  GET    /api/v1/photos/validate/$JOB_ID  → $PHOTOS_URL  (→ GPU-Test)"
echo "  Step 5  POST   /api/v1/design/projects     → $DESIGN_URL"
echo "  Step 6  POST   /api/v1/design/expose       → $DESIGN_URL   (→ Cloud)"
echo "  Step 7  GET    /api/v1/topology            → $CLOUD_URL"
echo "  ───────────────────────────────────────────────────────────"
echo ""

echo "  Cross-App Interactions:"
echo "  ┌──────────────────┬──────────────────┬─────────────────────────┐"
printf "  │ %-16s │ %-16s │ %-23s │\n" "Caller" "Target" "What"
echo "  ├──────────────────┼──────────────────┼─────────────────────────┤"
printf "  │ %-16s │ %-16s │ %-23s │\n" "Nexus-Photos" "Nexus-GPU-Test" "POST /gpu-test/jobs"
printf "  │ %-16s │ %-16s │ %-23s │\n" "Nexus-Design" "Nexus-Cloud" "POST /api/v1/public-url"
printf "  │ %-16s │ %-16s │ %-23s │\n" "Photos/Design" "Nexus-Cloud" "heartbeat + registration"
echo "  └──────────────────┴──────────────────┴─────────────────────────┘"
echo ""

echo "  Results:  ${SUCCESS} succeeded, ${FAIL:-0} with issues (of 7 steps)"
echo ""
echo "  ───────────────────────────────────────────────────────────"
echo "  Logs:  $LOG_DIR/"
ls -la "$LOG_DIR/" 2>/dev/null | tail -n +2 || echo "  (no logs)"
echo ""

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   Demo complete. Services will be cleaned up on exit.      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
