#!/usr/bin/env bash
# Production deploy script for nexus-mailsmtpd.
# Source apps/Nexus-Email/.env before running; it must never be committed.
set -euo pipefail

BINARY="${BINARY:-target/release/nexus-mailsmtpd}"
SERVICE="${SERVICE:-nexus-mailsmtpd}"

die() { echo "error: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Standard restart (no migration)
# ---------------------------------------------------------------------------
if [[ "${1:-}" != "mail-rollout" ]]; then
    systemctl restart "$SERVICE"
    echo "restarted $SERVICE"
    exit 0
fi

# ---------------------------------------------------------------------------
# Mail-body migration rollout
#
# Run this path once when enabling external-body storage for the first time,
# or after a schema migration that touches the external-body tables.
#
# Pre-conditions (operator must verify before running):
#   1. NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER is NOT set in .env yet.
#   2. The S3 bucket exists, versioning is enabled, and conditional writes work.
#   3. The database migration SQL has been reviewed and is ready to apply.
#
# This script stops mail writers, applies the migration, runs the capability
# probe, confirms zero legacy-body inventory, sets the marker, and restarts.
# ---------------------------------------------------------------------------

echo "=== mail-body migration rollout ==="

# Step 1: stop writers so no new mail arrives during the migration window.
echo "--- stopping $SERVICE"
systemctl stop "$SERVICE"

# Step 2: apply the migration.
echo "--- applying migration"
: "${NEXUS_EMAIL_DATABASE_URL:?must be set}"
for f in crates/nexus-mailstore/migrations/*.sql; do
    echo "    applying $f"
    psql "$NEXUS_EMAIL_DATABASE_URL" -v ON_ERROR_STOP=1 -f "$f"
done

# Step 3: confirm zero legacy-body inventory (messages with body_inline IS NOT
# NULL that still need migrating — adapt the WHERE clause to match your schema
# version if the column names differ).
echo "--- checking legacy-body inventory"
legacy_count=$(psql "$NEXUS_EMAIL_DATABASE_URL" -At -c \
    "SELECT COUNT(*) FROM messages WHERE body_inline IS NOT NULL AND external_body_id IS NULL")
if [[ "$legacy_count" -ne 0 ]]; then
    die "legacy-body inventory is not zero ($legacy_count rows); \
migrate all inline bodies to object storage before enabling the service"
fi
echo "    legacy-body inventory: $legacy_count rows (ok)"

# Step 4: run the capability probe to confirm the bucket is ready.
echo "--- running capability probe"
: "${S3_ENDPOINT:?must be set}"
: "${S3_BUCKET:?must be set}"
: "${S3_ACCESS_KEY:?must be set}"
: "${S3_SECRET_KEY:?must be set}"
# The binary will refuse to enable the object service without the marker;
# run with a temporary marker just for the probe.
NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=probe-only \
    "$BINARY" --capability-probe 2>&1 | tee /tmp/mail-capability-probe.log
if ! grep -q "capability probe: ok" /tmp/mail-capability-probe.log; then
    die "capability probe did not confirm ok; check /tmp/mail-capability-probe.log"
fi
echo "    capability probe passed"

# Step 5: set the migration marker permanently so the service starts.
echo "--- setting NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER in .env"
MARKER="migrated-$(date -u +%Y%m%dT%H%M%SZ)"
# Append or replace the marker line in .env (the file is sourced by the
# service unit, so changes take effect on restart).
if grep -q "^NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=" apps/Nexus-Email/.env 2>/dev/null; then
    sed -i "s|^NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=.*|NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=$MARKER|" \
        apps/Nexus-Email/.env
else
    echo "NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=$MARKER" >> apps/Nexus-Email/.env
fi
echo "    marker set: $MARKER"

# Step 6: restart with the new binary.
echo "--- restarting $SERVICE"
systemctl start "$SERVICE"
echo "=== mail-body rollout complete ==="
