#!/bin/bash
# Docker Compose Pre-Flight Check
# Run before 'docker compose up' in the Nexus Systems ecosystem.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "━━━ Docker Compose Pre-Flight Check ━━━"
echo ""

# ── Check Docker ──
if ! command -v docker &>/dev/null; then
  echo "✗ Docker not found. Install: https://docs.docker.com/engine/install/"
  exit 1
fi
if ! docker ps &>/dev/null 2>&1; then
  echo "✗ Docker socket not accessible. Try: sudo usermod -aG docker \$USER"
  exit 1
fi
echo "✓ Docker: $(docker --version)"

# ── Check Compose file ──
if ! docker compose version &>/dev/null 2>&1; then
  echo "✗ docker compose not available (need Docker 20.10+)"
  exit 1
fi
echo "✓ Docker Compose available"

# ── Validate compose syntax ──
if ! docker compose -f "$ROOT/docker-compose.yml" config --quiet 2>/dev/null; then
  echo "✗ Invalid compose file. Run: docker compose config"
  exit 1
fi
echo "✓ docker-compose.yml syntactically valid"

# ── Port availability ──
for port in 5432 6379 9000 9001 8787 3080 3074 3075 3096 3113; do
  if ss -tlnp | grep -q ":$port " 2>/dev/null; then
    echo "⚠ Port $port in use. Stop conflicting service first: lsof -ti:$port | xargs kill"
  fi
done
echo "✓ No port conflicts detected"

# ── Data directory ──
[ -d "$ROOT/data" ] || mkdir -p "$ROOT/data"
echo "✓ data/ directory: $(du -sh "$ROOT/data" 2>/dev/null | cut -f1)"

# ── Apps present ──
for app in Nexus-Cloud Nexus-Graphic Nexus-Design Nexus-Draw Nexus-Photos Nexus-Video; do
  [ -f "$ROOT/apps/$app/package.json" ] || { echo "✗ $app missing (needs package.json)"; exit 1; }
done
echo "✓ All 6 apps present"

echo ""
echo "━━━ Ready for: docker compose up -d ━━━"

echo ""
echo "Services that will start:"
echo "  postgres      PostgreSQL 16  → :5432"
echo "  redis          Redis 7       → :6379"
echo "  minio          MinIO S3      → :9000"
echo "  nexus-cloud    Cloud Control → :8787"
echo "  nexus-graphic  Graphic Suite → :3080"
echo "  nexus-design   Design Tool   → :3074"
echo "  nexus-draw     Whiteboard    → :3075"
echo "  nexus-photos   Photo Manager → :3096"
echo "  nexus-video    Video Platform→ :3113"
echo ""
echo "Commands:"
echo "  docker compose up -d          # start everything"
echo "  docker compose logs -f        # tail all logs"
echo "  docker compose down           # stop everything"
echo "  ./scripts/demo/capstone.sh    # verify ecosystem is alive"
