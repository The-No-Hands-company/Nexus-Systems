#!/bin/bash
# Nexus Email — unit/integration tests.
#
# The delivery/queue tests need a real Postgres. Credentials are never written
# here: they come from the environment (or the repo-root .env, which bun and
# this script both treat as the single source of truth).
set -euo pipefail
cd "$(dirname "$0")"

if [ -z "${NEXUS_EMAIL_TEST_DATABASE_URL:-}" ] && [ -f "../../.env" ]; then
    POSTGRES_PASSWORD="$(sed -n 's/^POSTGRES_PASSWORD=//p' ../../.env | head -1 | tr -d '\r')"
    export NEXUS_EMAIL_TEST_DATABASE_URL="postgres://nexus:${POSTGRES_PASSWORD}@localhost:5432/nexus"
fi

echo -n "nexus-email... "
cargo test --workspace 2>&1 | tail -5
echo "PASS"
