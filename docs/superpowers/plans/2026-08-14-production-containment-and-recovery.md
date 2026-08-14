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
- `apps/Nexus-Cloud/src/startup-summary.ts`: pure secret-free startup summary formatter.
- `apps/Nexus-Cloud/src/startup-summary.test.ts`: sentinel-secret non-disclosure regression.
- `apps/Nexus-Cloud/src/index.ts`: logs only the safe summary.
- `deploy/production/nexus-chat.Caddyfile`: production request-log header filtering.
- `apps/Nexus/Caddyfile`: app-owned equivalent header filtering.
- `deploy/production/tests/caddy-logging.test.ts`: structural regression for sensitive-header deletion.
- `deploy/production/processes.sh`: listener ownership and conservative PID reconciliation helpers.
- `deploy/production/tests/processes.test.sh`: fake-process/unit tests for reconciliation rules.
- `deploy/production/deploy.sh`: consumes reconciliation helpers in start, status, and stop.
- `deploy/production/cloudflared.compose.yml`: managed tunnel container using a mounted token file, never a token argument.
- `deploy/production/rotate-production-secrets.sh`: non-echoing, checkpointed Cloudflare/MinIO rotation and rollback orchestration.
- `.gitignore`: narrow runtime secret and rollback paths.

---

### Task 1: Portable Reaction Queries

**Files:**
- Modify: `apps/Nexus/crates/nexus-db/src/repository/reactions.rs`
- Create: `apps/Nexus/crates/nexus-db/tests/reactions_postgres.rs`

**Interfaces:**
- Consumes: `NEXUS_TEST_DATABASE_URL`, which must name a scratch PostgreSQL database and follows `identity_provisioning.rs` safeguards.
- Produces: unchanged public repository function signatures; SQL parameters remain strings accepted by `sqlx::AnyPool`.

- [ ] **Step 1: Write the ignored PostgreSQL lifecycle regression**

Create a scratch-database test that installs Any drivers, opens `NEXUS_TEST_DATABASE_URL`, creates isolated users/messages/reactions fixtures in a transaction, and asserts:

```rust
assert!(reactions::add_reaction(&pool, message_id, user_id, "👍").await?);
assert!(!reactions::add_reaction(&pool, message_id, user_id, "👍").await?);
assert_eq!(reactions::get_reaction_counts(&pool, message_id).await?[0].count, 1);
assert!(reactions::has_user_reacted(&pool, message_id, user_id, "👍").await?);
assert_eq!(reactions::get_reactors(&pool, message_id, "👍", 10).await?, vec![user_id]);
assert!(reactions::remove_reaction(&pool, message_id, user_id, "👍").await?);
assert!(!reactions::has_user_reacted(&pool, message_id, user_id, "👍").await?);
```

Use UUID-tagged fixture names and transaction rollback; refuse a URL whose database name lacks `test` or `scratch`.

- [ ] **Step 2: Run the regression and capture the UUID/text failure**

Run with a newly created scratch database:

```bash
NEXUS_TEST_DATABASE_URL="$SCRATCH_DATABASE_URL" cargo test -p nexus-db --test reactions_postgres -- --ignored --nocapture
```

Expected: FAIL on add with PostgreSQL reporting a UUID column versus text expression.

- [ ] **Step 3: Make all reaction SQL portable**

For insertion, select the already typed primary keys instead of assigning text parameters directly:

```sql
INSERT INTO reactions (message_id, user_id, emoji, created_at)
SELECT m.id, u.id, $3, CURRENT_TIMESTAMP
FROM messages m CROSS JOIN users u
WHERE CAST(m.id AS TEXT) = $1 AND CAST(u.id AS TEXT) = $2
ON CONFLICT (message_id, user_id, emoji) DO NOTHING
```

For predicates, replace every UUID comparison with `CAST(message_id AS TEXT) = $n` and `CAST(user_id AS TEXT) = $n`. In dynamic `IN` queries use `CAST(message_id AS TEXT) IN (...)`. In tuple-returning batch queries select `CAST(message_id AS TEXT) AS message_id` and `CAST(user_id AS TEXT) AS user_id` so AnyPool continues decoding strings.

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
git -C apps/Nexus add crates/nexus-db/src/repository/reactions.rs crates/nexus-db/tests/reactions_postgres.rs
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
```

Each test uses a temporary `PID_DIR`; it never reads or writes `/tmp/nexus-production/pids`.

- [ ] **Step 2: Run the PID tests red**

```bash
bash deploy/production/tests/processes.test.sh
```

Expected: FAIL because the helper file does not exist.

- [ ] **Step 3: Implement ownership checks**

Resolve listeners with `ss -H -ltnp "sport = :$port"`, require exactly one PID, require `kill -0`, require `/proc/$pid/cwd` to equal the expected directory, and require `/proc/$pid/cmdline` to contain the exact executable pattern. Write PID files atomically with `umask 077`, a temporary sibling, and `mv`.

In `start_service`, call `reconcile_pid` before starting; return success only after safe adoption. Treat an occupied but unverifiable port as a hard conflict. In `status`, show `managed`, `conflict`, or `not running`. In `stop`, kill only `validated_pid` output and otherwise leave the process untouched.

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
- Modify: `.gitignore`
- Test: `deploy/production/tests/rotation.test.sh`

**Interfaces:**
- Consumes: protected `CF_API_TOKEN`, discovered Cloudflare account ID, existing tunnel ID, root `.env`, and `apps/Nexus-Cloud/.env`.
- Produces: `/tmp/nexus-production/secrets/cloudflared.token` mode `0600`; a managed `cloudflared` container whose argv contains only `--token-file /run/secrets/tunnel-token`.

- [ ] **Step 1: Write command-capture rotation tests**

Override `curl`, `docker`, `openssl`, and `install` with recorders. Assert secrets occur only in protected input files/stdin, never command arguments or captured stdout. Assert failure before the MinIO health checkpoint restores both environment files and recreates MinIO with matching old credentials.

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

The script uses `set -euo pipefail`, `umask 077`, `mktemp -d /tmp/nexus-production/rotation.XXXXXX`, restores on `ERR` until each explicit checkpoint, and edits environment files without echoing values. It derives the Cloudflare account ID from the authenticated zone response, rotates the tunnel secret with `PATCH /accounts/{account}/cfd_tunnel/{tunnel}` using a new 32-byte base64 secret, fetches the replacement token with the documented token endpoint, and force-disconnects previous connections after the new token file is ready. References: Cloudflare's official tunnel-token and tunnel-update API documentation.

- [ ] **Step 4: Run tests and static checks**

```bash
bash deploy/production/tests/rotation.test.sh
bash -n deploy/production/rotate-production-secrets.sh
docker compose -f deploy/production/cloudflared.compose.yml config >/dev/null
git diff --check
```

- [ ] **Step 5: Commit and push all software changes**

```bash
git add .gitignore deploy/production/cloudflared.compose.yml deploy/production/rotate-production-secrets.sh deploy/production/tests/rotation.test.sh
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

Run the rotation script's `prepare` phase. Record only file hashes, modes, service/container IDs, health status, and HTTP codes. Confirm rollback copies are mode `0600` in the temporary directory.

- [ ] **Step 2: Deploy containment first**

Restart Nexus Cloud and Nexus Chat Caddy only. Send a disposable authenticated request, then scan fresh logs for fixed sentinel strings and sensitive header names. Stop if Cloud still logs storage fields or Caddy logs request headers.

- [ ] **Step 3: Rotate and recreate cloudflared**

Run the script's `rotate-tunnel` phase. Confirm the replacement container is healthy, public routes return expected codes, its command contains `--token-file` but no token-shaped argument, and Cloudflare reports only replacement connections. Force-disconnect old connections as documented by Cloudflare.

- [ ] **Step 4: Rotate MinIO and Cloud together**

Run the `rotate-storage` phase. Recreate `nexus-systems-minio-1`, wait for Docker health, restart Cloud, and perform a disposable S3 write/read/delete probe under the configured bucket prefix. On failure, allow the scripted paired rollback; do not delete or recreate the volume.

- [ ] **Step 5: Remove compromised historical logs**

After old credentials are invalid, stop the two affected log writers briefly, remove only the identified Cloud and Nexus Chat Caddy runtime log files, recreate empty files if their launch method requires them, and restart. Do not touch database, object-storage, audit, or unrelated application logs.

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

Securely remove the rotation temporary directory only after every check passes. Report commit IDs, restart windows, HTTP status results, PID/listener matches, reaction/storage probe outcomes, invalidation results, and any remaining degradation. Never include credential material.

---

## Self-Review

- Spec coverage: Tasks 1–5 cover reaction repair, PID reconciliation, Cloud/Caddy/tunnel containment, both rotations, log cleanup, rollback, and full verification. Least-privilege MinIO accounts remain explicitly deferred.
- Placeholder scan: no deferred implementation placeholders are present; every task names files, interfaces, commands, expected failures, and completion evidence.
- Type consistency: repository APIs remain unchanged; `startupSummary` is the sole new TypeScript interface; shell helper names match their deployer consumers; runtime phase names match the rotation script contract.
