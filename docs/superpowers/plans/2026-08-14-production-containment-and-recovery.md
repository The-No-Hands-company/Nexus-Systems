# Production Containment and Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop production credential and identity-token leakage, repair reactions and PID ownership, rotate the compromised Cloudflare tunnel and MinIO credentials with short coordinated restarts, and prove the stack healthy.

**Architecture:** Code containment lands before any credential changes. Each software fix has a red/green test cycle and its own commit; live rotation is a checkpointed runbook that changes one credential class at a time, preserves rollback material only in a protected temporary directory, and never writes secret values to stdout, Git, process arguments, or logs.

**Tech Stack:** Rust 1.93 nightly, sqlx AnyPool, PostgreSQL 16, SQLite, Bun 1.3.12, TypeScript, Bash, Caddy 2, Docker Compose, MinIO, Cloudflare Tunnel API.

## Global Constraints

- Never print, commit, or copy a tunnel token, S3 access key, S3 secret key, identity JWT, API key, cookie, or rollback credential into a log.
- Do not delete Docker volumes, MinIO buckets, PostgreSQL rows, Nexus data, `apps/Nexus-Modeling/build/`, `VersaAI-LegecyOnly-do-not-touch/`, or `Backups/`.
- Preserve `sqlx::AnyPool` compatibility with PostgreSQL and SQLite.
- Adopt or stop only a PID proven to own the service's expected listener; never use broad `pkill` or process-name matching.
- Keep old credentials only in an owner-readable temporary rollback directory and remove it after successful invalidation checks.
- Production is not fully healthy until scoped gates, authenticated reaction/storage probes, public checks, PID ownership, and leakage scans all pass.
- Restricted MinIO service accounts are a separate follow-up task.

---

## File Map

- `apps/Nexus/crates/nexus-db/src/repository/reactions.rs`: portable UUID SQL for every reaction operation.
- `apps/Nexus/crates/nexus-db/tests/reactions_postgres.rs`: ignored scratch-Postgres regression covering the complete reaction lifecycle.
- `apps/Nexus/crates/nexus-api/src/routes/messages.rs`: target-bound bulk reaction-delete authorization and cross-channel regressions.
- `apps/Nexus-Cloud/src/startup-summary.ts`: pure secret-free startup summary formatter.
- `apps/Nexus-Cloud/src/startup-summary.test.ts`: sentinel-secret non-disclosure regression.
- `apps/Nexus-Cloud/src/index.ts`: logs only the safe summary.
- `apps/Nexus-Cloud/src/storage/index.ts`: local shared pools resolve current protected credentials and do not persist new raw keys.
- `apps/Nexus-Cloud/src/storage/index.test.ts`: stale legacy-pool credential regression.
- `apps/Nexus-Cloud/src/storage/s3.ts`: abort-signal support for bounded authenticated probes.
- `deploy/production/nexus-chat.Caddyfile`: production request-log header filtering.
- `apps/Nexus/Caddyfile`: app-owned equivalent header filtering.
- `deploy/production/tests/caddy-logging.test.ts`: structural regression for sensitive-header deletion.
- `deploy/production/processes.sh`: listener ownership and conservative PID reconciliation helpers.
- `deploy/production/tests/processes.test.sh`: fake-process/unit tests for reconciliation, restart, log, and HTTP-health rules.
- `deploy/production/deploy.sh`: consumes reconciliation helpers in start, status, stop, safe Cloud/Chat/Caddy restart, and checkpoint-gated log cleanup.
- `deploy/production/start-cloud.sh`: protected Cloud environment loader with no secret-bearing launcher arguments.
- `deploy/production/cloudflared.compose.yml`: managed tunnel container using a mounted token file, never a token argument.
- `deploy/production/rotate-production-secrets.sh`: non-echoing, checkpointed Cloudflare/MinIO rotation and rollback orchestration.
- `deploy/production/storage-probe.ts`: secret-free authenticated disposable S3 write/read/delete probe.
- `deploy/production/tests/storage-probe.test.ts`: probe lifecycle, cleanup, and bucket-scope regressions.
- `.gitignore`: narrow runtime secret and rollback paths.

---

### Task 1: Portable Reaction Queries

**Files:**
- Modify: `apps/Nexus/crates/nexus-db/src/repository/reactions.rs`
- Create: `apps/Nexus/crates/nexus-db/tests/reactions_postgres.rs`
- Modify: `apps/Nexus/crates/nexus-api/src/routes/messages.rs`

**Interfaces:**
- Consumes: `NEXUS_TEST_DATABASE_URL`, which must name a scratch PostgreSQL database and follows `identity_provisioning.rs` safeguards.
- Produces: unchanged public repository function signatures; PostgreSQL casts UUID bound parameters while keeping indexed columns unchanged, SQLite remains valid, and both bulk route authorizations bind to the resolved target channel/server.

- [ ] **Step 1: Write the ignored PostgreSQL lifecycle regression**

Create a scratch-database test that installs Any drivers, opens `NEXUS_TEST_DATABASE_URL`, creates isolated users/messages/reactions fixtures, and covers add/duplicate/count/reactors/remove plus batch counts, batch user lookups, emoji moderation delete, and all-reactions moderation delete. Use UUID-tagged fixtures and guaranteed cleanup; refuse a URL whose database name lacks `test` or `scratch`.

Add API tests in which an owner-authorized URL channel and the target message's actual channel are different. Assert both bulk DELETE handlers return not-found and leave all target reactions unchanged.

The single-operation lifecycle includes:

```rust
assert!(reactions::add_reaction(&pool, message_id, user_id, "👍").await?);
assert!(!reactions::add_reaction(&pool, message_id, user_id, "👍").await?);
assert_eq!(reactions::get_reaction_counts(&pool, message_id).await?[0].count, 1);
assert!(reactions::has_user_reacted(&pool, message_id, user_id, "👍").await?);
assert_eq!(reactions::get_reactors(&pool, message_id, "👍", 10).await?, vec![user_id]);
assert!(reactions::remove_reaction(&pool, message_id, user_id, "👍").await?);
assert!(!reactions::has_user_reacted(&pool, message_id, user_id, "👍").await?);
```

- [ ] **Step 2: Run the regression and capture the UUID/text failure**

Run with a newly created scratch database:

```bash
NEXUS_TEST_DATABASE_URL="$SCRATCH_DATABASE_URL" cargo test -p nexus-db --test reactions_postgres -- --ignored --nocapture
```

Expected: FAIL on add with PostgreSQL reporting a UUID column versus text expression.

- [ ] **Step 3: Make all reaction SQL portable**

Detect the `AnyPool` backend. PostgreSQL leaves indexed UUID columns unchanged and casts each bound UUID value:

```sql
INSERT INTO reactions (message_id, user_id, emoji, created_at)
VALUES ($1::uuid, $2::uuid, $3, CURRENT_TIMESTAMP)
ON CONFLICT (message_id, user_id, emoji) DO NOTHING

DELETE FROM reactions WHERE message_id = $1::uuid AND user_id = $2::uuid
```

SQLite emits `$1`, `$2`, and so on without casts. Dynamic PostgreSQL `IN` lists use `$n::uuid` for every value and never cast `message_id` or `user_id`, preserving index use. Tuple outputs may select `CAST(message_id AS TEXT) AS message_id` and `CAST(user_id AS TEXT) AS user_id` so `AnyPool` continues decoding strings.

For each bulk DELETE route, resolve the target message joined to its actual channel/server. Reject when the URL channel differs from the resolved channel, then authorize the resolved target server/channel before calling the repository delete.

- [ ] **Step 4: Run PostgreSQL and SQLite gates**

```bash
NEXUS_TEST_DATABASE_URL="$SCRATCH_DATABASE_URL" cargo test -p nexus-db --test reactions_postgres -- --ignored --nocapture
cargo test -p nexus-db --lib
cargo check -p nexus-db
git -C apps/Nexus diff --check
```

Expected: all pass; the scratch database contains no committed fixture rows.

- [ ] **Step 5: Commit the Nexus submodule change and update the parent gitlink**

```bash
git -C apps/Nexus add crates/nexus-api/src/routes/messages.rs crates/nexus-db/src/repository/reactions.rs crates/nexus-db/tests/reactions_postgres.rs
git -C apps/Nexus commit -m "fix(db): make reaction UUID queries portable"
git add apps/Nexus
git commit -m "fix(chat): repair reaction persistence"
```

---

### Task 2: Secret-Free Cloud and Caddy Logging

**Files:**
- Create: `apps/Nexus-Cloud/src/startup-summary.ts`
- Create: `apps/Nexus-Cloud/src/startup-summary.test.ts`
- Modify: `apps/Nexus-Cloud/src/index.ts`
- Modify: `deploy/production/nexus-chat.Caddyfile`
- Modify: `apps/Nexus/Caddyfile`
- Create: `deploy/production/tests/caddy-logging.test.ts`

**Interfaces:**
- Produces: `startupSummary(snapshot: ControlPlaneSnapshot): SafeStartupSummary`; the returned object contains counts and non-sensitive identifiers only.
- Produces: Caddy filter encoders that delete the entire `request.headers` field before JSON or console encoding.

- [ ] **Step 1: Write Cloud sentinel-secret tests**

Construct a snapshot containing sentinels in storage pool access key, secret key, endpoint, and nested metadata. Assert the serialized summary contains none of them and includes stable counts:

```ts
const encoded = JSON.stringify(startupSummary(snapshot));
for (const secret of ["ACCESS_SENTINEL", "SECRET_SENTINEL", "TOKEN_SENTINEL", "http://private:9000"])
  expect(encoded).not.toContain(secret);
expect(startupSummary(snapshot)).toMatchObject({ storagePoolCount: 1 });
```

- [ ] **Step 2: Run the Cloud test red**

```bash
cd apps/Nexus-Cloud && bun test src/startup-summary.test.ts
```

Expected: FAIL because `startup-summary.ts` does not exist.

- [ ] **Step 3: Implement an allowlisted startup summary**

Return only explicit scalar counts and booleans. Do not clone, spread, recursively redact, or stringify arbitrary snapshot branches. Replace this line in `index.ts`:

```ts
console.log("State snapshot:", JSON.stringify(controlPlaneService.snapshot()));
```

with:

```ts
console.log("State summary:", JSON.stringify(startupSummary(controlPlaneService.snapshot())));
```

- [ ] **Step 4: Write and run Caddy logging regressions**

The Bun test reads both Caddyfiles and requires the access-log encoder to contain:

```caddyfile
format filter {
    fields {
        request>headers delete
    }
    wrap console
}
```

Use `wrap json` for file JSON logging. The test must fail if either file has bare `format console`/`format json` or lacks `request>headers delete`.

Run:

```bash
cd deploy/production && bun test tests/caddy-logging.test.ts
caddy validate --config deploy/production/nexus-chat.Caddyfile --adapter caddyfile
caddy validate --config apps/Nexus/Caddyfile --adapter caddyfile
```

- [ ] **Step 5: Run complete scoped gates and commit**

```bash
cd apps/Nexus-Cloud && bun test src && bun run typecheck
cd deploy/production && bun test tests/ && bunx tsc --noEmit
git diff --check
git add apps/Nexus-Cloud/src/index.ts apps/Nexus-Cloud/src/startup-summary.ts apps/Nexus-Cloud/src/startup-summary.test.ts deploy/production/nexus-chat.Caddyfile deploy/production/tests/caddy-logging.test.ts apps/Nexus
git commit -m "fix(security): redact production request and startup logs"
```

Commit the app-owned Caddyfile inside `apps/Nexus` first, then stage the updated parent gitlink.

---

### Task 3: Conservative PID Reconciliation

**Files:**
- Create: `deploy/production/processes.sh`
- Create: `deploy/production/tests/processes.test.sh`
- Modify: `deploy/production/deploy.sh`

**Interfaces:**
- Produces: `listener_pid PORT`, `pid_matches_service PID PORT DIR EXEC_PATTERN`, `reconcile_pid NAME PORT DIR EXEC_PATTERN`, and `validated_pid NAME PORT DIR EXEC_PATTERN`.
- Returns: exit `0` with exactly one PID on stdout when ownership is proven; nonzero with diagnostics on stderr otherwise.

- [ ] **Step 1: Write fake-command unit tests**

Source `processes.sh` with `SS_BIN`, `PS_BIN`, and `READLINK_BIN` overridden by fixtures. Cover:

```bash
test_adopts_one_matching_listener
test_rejects_foreign_listener
test_replaces_stale_pid_file
test_rejects_multiple_listener_pids
test_validated_pid_rejects_pid_that_no_longer_owns_port
test_stop_refuses_a_mismatched_pid_without_signalling_it
test_start_appends_to_existing_service_log
test_http_health_requires_curl_fail_and_semantic_success
test_caddy_restart_validates_config_before_stopping
test_cloud_restart_refuses_a_mismatched_managed_pid
test_sensitive_log_reset_requires_both_rotation_checkpoints
```

Each test uses a temporary `PID_DIR`; it never reads or writes `/tmp/nexus-production/pids`.

- [ ] **Step 2: Run the PID tests red**

```bash
bash deploy/production/tests/processes.test.sh
```

Expected: FAIL because the helper file does not exist.

- [ ] **Step 3: Implement ownership checks**

Resolve listeners with `ss -H -ltnp "sport = :$port"`, require exactly one PID, require `kill -0`, require `/proc/$pid/cwd` to equal the expected directory, and require `/proc/$pid/cmdline` to contain the exact executable pattern. Write PID files atomically with `umask 077`, a temporary sibling, and `mv`.

In `start_service`, call `reconcile_pid` before starting, then use a bounded reconciliation loop for the launched process. If it misses the checkpoint, revalidate that exact PID's cwd/command before stopping it; `stop` may safely adopt an exact late listener before signalling it. Treat an occupied but unverifiable port as a hard conflict and append to the existing log until explicit post-invalidation cleanup. In `status`, show `managed`, `conflict`, or `not running`; HTTP checks use `curl --fail` and accept only exact plain `ok`/`healthy` or semantic JSON success. In `stop`, kill only validated or exactly reconciled output and otherwise leave the process untouched.

Expose `restart cloud`, `restart nexus-chat`, and `restart nexus-chat-web`. Cloud uses `start-cloud.sh` to clear inherited credential aliases and source the protected Cloud file after `setsid`/`nohup`, so secrets never appear as `KEY=value` launcher arguments. Require the protected Cloud/Chat configuration before stopping either backend and validate the Caddyfile before stopping Caddy. Expose `cleanup-log` only for Cloud/Caddy, require both durable rotation checkpoints, stop only the validated PID, atomically replace only the exact log with a mode-`0600` empty file, and start only that service.

- [ ] **Step 4: Run unit and production-script syntax gates**

```bash
bash deploy/production/tests/processes.test.sh
bash -n deploy/production/processes.sh deploy/production/deploy.sh
cd deploy/production && bun test tests/ && bunx tsc --noEmit
git diff --check
```

- [ ] **Step 5: Commit**

```bash
git add deploy/production/processes.sh deploy/production/tests/processes.test.sh deploy/production/deploy.sh
git commit -m "fix(deploy): reconcile service PID ownership"
```

---

### Task 4: Token-File Tunnel Management and Rotation Script

**Files:**
- Create: `deploy/production/cloudflared.compose.yml`
- Create: `deploy/production/rotate-production-secrets.sh`
- Create: `deploy/production/storage-probe.ts`
- Modify: `.gitignore`
- Test: `deploy/production/tests/rotation.test.sh`
- Test: `deploy/production/tests/storage-probe.test.ts`

**Interfaces:**
- Consumes: protected `CF_API_TOKEN`, operator-completed Cloudflare dashboard rotation, required `NEXUS_ROTATION_TUNNEL_VERIFY_URL`, discovered Cloudflare account ID, existing tunnel ID, root `.env`, and `apps/Nexus-Cloud/.env`.
- Produces: `/tmp/nexus-production/secrets/cloudflared.token` mode `0600`; a managed `cloudflared` container whose argv contains only `--token-file /run/secrets/tunnel-token`.

- [ ] **Step 1: Write command-capture rotation tests**

Override `curl`, `docker`, `openssl`, and `install` with recorders. Assert secrets occur only in protected input files/stdin, never command arguments or captured stdout. Cover protected bootstrap from the current connector argv when no token file exists, changed-token GET/adoption/re-fetch/hash verification, failure recovery, explicit storage rollback after its MinIO checkpoint, the optional deployed-Chat pair, preservation/refusal of concurrent environment edits, and cleanup refusal until both checkpoints exist. Assert failure before the MinIO health checkpoint restores the atomic credential-key set and recreates MinIO with matching old credentials.

- [ ] **Step 2: Run tests red**

```bash
bash deploy/production/tests/rotation.test.sh
```

- [ ] **Step 3: Add tunnel Compose and checkpointed script**

The Compose service mounts the token file read-only and runs:

```yaml
command: ["tunnel", "--no-autoupdate", "run", "--token-file", "/run/secrets/tunnel-token"]
restart: unless-stopped
```

The script uses `set -euo pipefail`, `umask 077`, bounded Cloudflare/public requests, `mktemp -d /tmp/nexus-production/rotation.XXXXXX`, restores on `ERR` until each explicit checkpoint, and edits environment files without echoing values. `prepare` can inspect the exact running connector command into a protected file, extract the one current token into the protected token file, hash it, and remove the argv snapshot without printing it. The operator—not the script—rotates the tunnel credential in the Cloudflare dashboard. `rotate-tunnel` derives the account ID from the authenticated zone response, requires `NEXUS_ROTATION_TUNNEL_VERIFY_URL`, retrieves the dashboard-issued token with the documented GET endpoint, requires its hash to differ from the prepared token, adopts it, verifies the replacement connector and public route, re-fetches it, and requires the remote/staged/installed hashes to match before disconnecting old connections. Storage rotation conditionally includes the deployed Nexus Chat environment. `rollback-storage` remains callable after the MinIO health checkpoint and restores only the six affected credential keys when their current values match the prepared old or expected new values, preserving unrelated edits.

- [ ] **Step 4: Run tests and static checks**

```bash
bash deploy/production/tests/rotation.test.sh
bun test deploy/production/tests/storage-probe.test.ts
bash -n deploy/production/rotate-production-secrets.sh
docker compose -f deploy/production/cloudflared.compose.yml config >/dev/null
git diff --check
```

- [ ] **Step 5: Commit and push all software changes**

```bash
git add .gitignore deploy/production/cloudflared.compose.yml deploy/production/rotate-production-secrets.sh deploy/production/storage-probe.ts deploy/production/tests/rotation.test.sh deploy/production/tests/storage-probe.test.ts
git commit -m "fix(security): manage production credential rotation"
git push origin main
```

---

### Task 5: Execute Coordinated Production Recovery

**Files:**
- Runtime only: root `.env`, `apps/Nexus-Cloud/.env`, `/tmp/nexus-production/secrets/`, `/tmp/nexus-production/*.log`, Docker containers, `/tmp/nexus-production/pids/`.

**Interfaces:**
- Consumes: Tasks 1–4 committed and passing.
- Produces: rotated credentials, contained logs, accurate PIDs, healthy public services, and an evidence-only completion report without secret values.

- [ ] **Step 1: Establish rollback and baseline without printing secrets**

Require and export `NEXUS_ROTATION_TUNNEL_VERIFY_URL` as the real public route used for post-adoption verification, then run the rotation script's `prepare` phase:

```bash
: "${NEXUS_ROTATION_TUNNEL_VERIFY_URL:?export the required public tunnel verification URL}"
export NEXUS_ROTATION_TUNNEL_VERIFY_URL
bash deploy/production/rotate-production-secrets.sh prepare
```

If the protected token file does not exist yet, `prepare` bootstraps the current token from a protected exact-container argv snapshot and deletes that snapshot without printing it. It also includes the deployed Nexus Chat environment when present. Record only file hashes, modes, service/container IDs, health status, and HTTP codes. Confirm every rollback copy is mode `0600` in the temporary directory.

- [ ] **Step 2: Deploy containment first**

Record the current byte length of both logs, then use the validated per-service paths so the containment restart appends instead of truncating the compromised history:

```bash
bash deploy/production/deploy.sh restart cloud
bash deploy/production/deploy.sh restart nexus-chat-web
```

Send a disposable authenticated request, then scan only bytes written after the recorded offsets for fixed sentinel strings and sensitive header names. Stop if Cloud still logs storage fields or Caddy logs request headers. Do not remove historical logs yet.

- [ ] **Step 3: Rotate and recreate cloudflared**

Pause here for the operator to rotate the existing tunnel token explicitly in the Cloudflare dashboard. The recovery script does not mutate the remote tunnel credential. After the dashboard reports rotation complete, retain the required verification URL in the environment and run:

```bash
: "${NEXUS_ROTATION_TUNNEL_VERIFY_URL:?export the required public tunnel verification URL}"
bash deploy/production/rotate-production-secrets.sh rotate-tunnel
```

The phase retrieves the dashboard-issued token with GET, requires a changed hash, adopts it, verifies the replacement connector and public URL, re-fetches it, and requires the remote/staged/installed hashes to agree before deleting only the previously observed connections. Confirm the replacement container is healthy, public routes return expected codes, its command contains `--token-file` but no token-shaped argument, and Cloudflare reports only replacement connections.

- [ ] **Step 4: Rotate MinIO and every deployed consumer together**

Export `NEXUS_ROTATION_S3_PROBE_BUCKET` as an existing disposable-probe bucket under the configured prefix. `prepare` has conditionally included `deploy/production/nexus-chat.env` in the protected atomic set when that deployment exists. Run the following as one `set -euo pipefail` shell block so any Cloud/Chat restart, semantic health, deadline, or authenticated S3 write/read/delete failure automatically invokes post-checkpoint rollback and restarts each deployed consumer against the restored credentials:

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
    : # checkpoint verified through Cloud, Chat when deployed, and bounded authenticated S3
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

Each probe has separate bounded primary and cleanup deadlines. The 300-second outer guard exceeds the maximum 120-second primary plus 120-second cleanup contract, so it cannot preempt deletion after an ambiguous PUT. Probes emit only pass/fail. Chat uses its configured bucket. Cloud shared-pool provisioning resolves the current protected credentials instead of legacy persisted raw keys and is covered by the Cloud test gate. Do not delete or recreate the MinIO volume. Rollback changes only the six prepared credential keys, refuses to overwrite a concurrent credential edit, preserves unrelated lines, and removes the storage checkpoint; investigate and rerun before log or rotation cleanup.

- [ ] **Step 5: Remove compromised historical logs**

After both tunnel and storage checkpoints exist and old credentials are proven invalid, use the checkpoint-gated exact-log procedure:

```bash
bash deploy/production/deploy.sh cleanup-log cloud
bash deploy/production/deploy.sh cleanup-log nexus-chat-web
```

Each command validates the replacement service configuration, stops only its validated PID, atomically installs an empty mode-`0600` exact log, and restarts only that service. Do not touch database, object-storage, audit, or unrelated application logs.

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

Run `bash deploy/production/rotate-production-secrets.sh cleanup` only after every check passes; the command refuses unless both tunnel and storage checkpoint files exist. Report commit IDs, restart windows, HTTP status results, PID/listener matches, reaction/storage probe outcomes, invalidation results, and any remaining degradation. Never include credential material.

---

## Self-Review

- Spec coverage: Tasks 1–5 cover reaction repair, PID reconciliation, Cloud/Caddy/tunnel containment, both rotations, log cleanup, rollback, and full verification. Least-privilege MinIO accounts remain explicitly deferred.
- Placeholder scan: no deferred implementation placeholders are present; every task names files, interfaces, commands, expected failures, and completion evidence.
- Type consistency: repository APIs remain unchanged; `startupSummary` is the sole new TypeScript interface; shell helper names match their deployer consumers; runtime phase names match the rotation script contract.
