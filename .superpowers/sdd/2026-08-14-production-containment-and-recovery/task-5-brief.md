### Task 5: Execute Coordinated Production Recovery

**Files:**
- Runtime only: root `.env`, `apps/Nexus-Cloud/.env`, deployed `deploy/production/nexus-chat.env`, `/tmp/nexus-production/secrets/`, `/tmp/nexus-production/*.log`, Docker containers, `/tmp/nexus-production/pids/`.

**Interfaces:**
- Consumes: Tasks 1–4 committed and passing.
- Produces: rotated credentials, contained logs, accurate PIDs, healthy public services, and an evidence-only completion report without secret values.

- [ ] **Step 1: Establish rollback and baseline without printing secrets**

Require the real public tunnel verification URL and preserve it for the later tunnel phase:

```bash
: "${NEXUS_ROTATION_TUNNEL_VERIFY_URL:?export the required public tunnel verification URL}"
export NEXUS_ROTATION_TUNNEL_VERIFY_URL
bash deploy/production/rotate-production-secrets.sh prepare
```

If the protected token file does not exist yet, `prepare` bootstraps the current token from a protected exact-container argv snapshot and deletes that snapshot without printing it. It also includes the deployed Nexus Chat environment when present. Record only file hashes, modes, service/container IDs, health status, and HTTP codes. Confirm every rollback copy is mode `0600` in the temporary directory.

- [ ] **Step 2: Deploy containment first**

Record the current byte length of both affected logs, then run only the validated per-service restart paths:

```bash
bash deploy/production/deploy.sh restart cloud
bash deploy/production/deploy.sh restart nexus-chat-web
```

Starts append and therefore preserve compromised history until post-invalidation cleanup. Send a disposable authenticated request, then scan only bytes written after the recorded offsets for fixed sentinel strings and sensitive header names. Stop if Cloud still logs storage fields or Caddy logs request headers.

- [ ] **Step 3: Rotate and recreate cloudflared**

Pause here for the operator to rotate the existing tunnel token explicitly in the Cloudflare dashboard. The script does not mutate the remote tunnel credential. Only after the dashboard reports rotation complete run:

```bash
: "${NEXUS_ROTATION_TUNNEL_VERIFY_URL:?export the required public tunnel verification URL}"
bash deploy/production/rotate-production-secrets.sh rotate-tunnel
```

The phase retrieves the dashboard-issued token with GET, requires a changed hash, adopts it, verifies the replacement connector and required public URL, re-fetches it, and requires the remote/staged/installed hashes to match before deleting only the previously observed connections. Confirm the replacement container is healthy, public routes return expected codes, its command contains `--token-file` but no token-shaped argument, and Cloudflare reports only replacement connections.

- [ ] **Step 4: Rotate MinIO and every deployed consumer together**

Export `NEXUS_ROTATION_S3_PROBE_BUCKET` as an existing disposable-probe bucket under the configured prefix. `prepare` conditionally included deployed `nexus-chat.env` in the atomic set. Run this as one shell block; any Cloud/Chat restart, semantic health, deadline, or authenticated S3 failure automatically invokes `rollback-storage` and restarts every deployed consumer against the restored pair:

```bash
set -euo pipefail
: "${NEXUS_ROTATION_S3_PROBE_BUCKET:?export an existing probe bucket under the configured prefix}"
export NEXUS_ROTATION_S3_PROBE_BUCKET
bash deploy/production/rotate-production-secrets.sh rotate-storage

probe_cloud_storage() (
    unset NEXUS_STORAGE_S3_ACCESS_KEY NEXUS_STORAGE_S3_SECRET_KEY \
        NEXUS__STORAGE__ACCESS_KEY NEXUS__STORAGE__SECRET_KEY
    set -a
    . apps/Nexus-Cloud/.env
    set +a
    export NEXUS_STORAGE_S3_ENDPOINT="${NEXUS_STORAGE_S3_ENDPOINT:-http://localhost:9000}"
    export NEXUS_STORAGE_S3_REGION="${NEXUS_STORAGE_S3_REGION:-us-east-1}"
    export NEXUS_STORAGE_S3_BUCKET_PREFIX="${NEXUS_STORAGE_S3_BUCKET_PREFIX:-nexus}"
    timeout 300s bun run deploy/production/storage-probe.ts
)

verify_deployed_chat_storage() {
    [ -f deploy/production/nexus-chat.env ] || return 0
    bash deploy/production/deploy.sh restart nexus-chat \
        && ( . deploy/production/deploy.sh; \
            http_endpoint_healthy http://localhost:8180/api/v1/health ) \
        && (
            unset NEXUS_STORAGE_S3_ENDPOINT NEXUS_STORAGE_S3_ACCESS_KEY \
                NEXUS_STORAGE_S3_SECRET_KEY NEXUS_ROTATION_S3_PROBE_BUCKET
            set -a
            . deploy/production/nexus-chat.env
            set +a
            export NEXUS_STORAGE_S3_ENDPOINT="$NEXUS__STORAGE__ENDPOINT"
            export NEXUS_STORAGE_S3_ACCESS_KEY="$NEXUS__STORAGE__ACCESS_KEY"
            export NEXUS_STORAGE_S3_SECRET_KEY="$NEXUS__STORAGE__SECRET_KEY"
            export NEXUS_STORAGE_S3_REGION="${NEXUS__STORAGE__REGION:-us-east-1}"
            export NEXUS_STORAGE_S3_BUCKET_PREFIX="${NEXUS_ROTATION_CHAT_S3_BUCKET_PREFIX:-nexus}"
            export NEXUS_ROTATION_S3_PROBE_BUCKET="$NEXUS__STORAGE__BUCKET"
            timeout 300s bun run deploy/production/storage-probe.ts
        )
}

if bash deploy/production/deploy.sh restart cloud \
    && ( . deploy/production/deploy.sh; http_endpoint_healthy http://localhost:8787/health ) \
    && probe_cloud_storage \
    && verify_deployed_chat_storage; then
    : # verified through Cloud, deployed Chat, and bounded authenticated S3
else
    recovery_status=0
    if bash deploy/production/rotate-production-secrets.sh rollback-storage; then
        bash deploy/production/deploy.sh restart cloud || recovery_status=1
        ( . deploy/production/deploy.sh; \
            http_endpoint_healthy http://localhost:8787/health ) || recovery_status=1
        if [ -f deploy/production/nexus-chat.env ]; then
            bash deploy/production/deploy.sh restart nexus-chat || recovery_status=1
        fi
    else
        recovery_status=1
    fi
    [ "$recovery_status" -eq 0 ] || \
        printf 'storage rollback/recovery requires immediate operator attention\n' >&2
    exit 1
fi
```

Each probe has separate bounded primary and cleanup deadlines. The 300-second outer guard exceeds the maximum 120-second primary plus 120-second cleanup contract, so it cannot preempt deletion after an ambiguous PUT. Probes emit only pass/fail. Chat uses its configured bucket. Shared Cloud pools resolve current protected credentials, not legacy persisted keys. Do not delete or recreate the volume. Rollback changes only the prepared credential keys, refuses to overwrite concurrent credential edits, preserves unrelated lines, and removes the checkpoint; investigate and rerun before cleanup.

- [ ] **Step 5: Remove compromised historical logs**

After both checkpoints exist and old credentials are proven invalid, use the checkpoint-gated exact-log commands:

```bash
bash deploy/production/deploy.sh cleanup-log cloud
bash deploy/production/deploy.sh cleanup-log nexus-chat-web
```

Each validates the replacement configuration, stops only its validated PID, atomically installs an empty mode-`0600` exact log, and restarts only that service. Do not touch database, object-storage, audit, or unrelated application logs.

- [ ] **Step 6: Reconcile PID ownership**

Run `bash deploy/production/deploy.sh bg`. Verify all eight managed services report a validated PID that owns the expected listener. Any foreign or ambiguous listener is investigated rather than adopted or killed.

- [ ] **Step 7: Run final technical and functional verification**

```bash
git status --short
git diff --check
cd apps/Nexus-Cloud && bun test src && bun run typecheck
cd deploy/production && bun test tests/ && bunx tsc --noEmit
git -C apps/Nexus status --short
```

Then run Docker health, all local endpoints, all public endpoints, an authenticated disposable reaction add/read/remove cycle, and an authenticated disposable S3 write/read/delete cycle.

- [ ] **Step 8: Prove invalidation and absence**

Using comparisons that emit only pass/fail, verify the old tunnel token cannot create a new connector, old MinIO credentials receive access denied, replacement values are absent from argv/container metadata/fresh logs, and fresh Caddy logs contain no `Authorization`, `Cookie`, or `X-Nexus-Identity` field.

- [ ] **Step 9: Destroy rollback material and report**

Run `bash deploy/production/rotate-production-secrets.sh cleanup` only after every check passes; it refuses unless both tunnel and storage checkpoints exist. Report commit IDs, restart windows, HTTP status results, PID/listener matches, reaction/storage probe outcomes, invalidation results, and any remaining degradation. Never include credential material.

---

## Self-Review

- Spec coverage: Tasks 1–5 cover reaction repair, PID reconciliation, Cloud/Caddy/tunnel containment, both rotations, log cleanup, rollback, and full verification. Least-privilege MinIO accounts remain explicitly deferred.
- Placeholder scan: no deferred implementation placeholders are present; every task names files, interfaces, commands, expected failures, and completion evidence.
- Type consistency: repository APIs remain unchanged; `startupSummary` is the sole new TypeScript interface; shell helper names match their deployer consumers; runtime phase names match the rotation script contract.
