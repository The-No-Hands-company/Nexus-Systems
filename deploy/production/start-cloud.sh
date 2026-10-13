#!/usr/bin/env bash

# Load Cloud credentials from the protected file after the launcher chain, so
# secret values never appear in `env`, `setsid`, or `nohup` process arguments.
set -euo pipefail
umask 077

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CLOUD_ENV_FILE="${NEXUS_CLOUD_ENV_FILE:-$ROOT/apps/Nexus-Cloud/.env}"
CLOUD_BINARY_PATH="${NEXUS_CLOUD_BINARY_PATH:-bun}"

[ -f "$CLOUD_ENV_FILE" ] || {
    printf 'protected Nexus Cloud environment is missing\n' >&2
    exit 1
}

# The protected file is authoritative. Remove every supported inherited alias
# first, then source the file with automatic export.
unset NEXUS_CLOUD_API_KEY CF_API_TOKEN \
    NEXUS_STORAGE_S3_ACCESS_KEY NEXUS_STORAGE_S3_SECRET_KEY \
    NEXUS__STORAGE__ACCESS_KEY NEXUS__STORAGE__SECRET_KEY
set -a
# shellcheck source=/dev/null
. "$CLOUD_ENV_FILE"
set +a

: "${NEXUS_CLOUD_API_KEY:?protected Cloud API key is required}"
: "${NEXUS_STORAGE_S3_ACCESS_KEY:?protected Cloud storage access key is required}"
: "${NEXUS_STORAGE_S3_SECRET_KEY:?protected Cloud storage secret key is required}"

# Cloud heartbeats itself through the SDK. NEXUS_CLOUD_URL above is its public
# address (for discovery); the heartbeat must stay on loopback, or it goes out
# through the proxy and Cloud's own sign-in gate refuses it.
export NEXUS_CLOUD_INTERNAL_URL="http://127.0.0.1:${PORT:-8787}"

exec "$CLOUD_BINARY_PATH" run src/index.ts
